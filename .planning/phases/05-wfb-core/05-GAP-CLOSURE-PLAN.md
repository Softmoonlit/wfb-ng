---
phase: 05
type: gap_closure
plan_id: 05-gap
version: 2
based_on:
  - 05-VERIFICATION.md
  - 05-REVIEW.md
gaps_count: 12
created: 2026-04-28
revised: 2026-04-28
---

# Phase 5: Gap Closure Plan (修订版)

## 计划目标

修复 Phase 5 实现中的关键缺口，使 wfb_core 单进程架构能够编译、运行并通过集成测试。

**来源：**
- 验证报告 (05-VERIFICATION.md): 20/39 must-haves 验证通过，发现 19 个缺口
- 代码审查 (05-REVIEW.md): 1 个关键问题，7 个警告，5 个信息项

---

## 缺口修复清单

### 🔴 P0: 编译阻塞问题

#### Gap-01: ServerScheduler 构造函数不匹配 ✅ 已修复
**问题：** src/wfb_core.cpp:143 调用 `ServerScheduler scheduler(config.mcs, config.bandwidth)`，但构造函数不存在
**修复：**
1. 在 `src/server_scheduler.h` 添加双参数构造函数
2. 修复相关编译问题（resource_guard.h, radiotap_template.h, packet_queue.h, guard_interval.h）
3. 更新 Makefile 包含所有必要源文件
**验证：** ✅ `make wfb_core` 编译成功，`./wfb_core --help` 正常运行
**修改文件：**
- src/server_scheduler.h: 添加 ServerScheduler(int mcs, int bandwidth) 构造函数
- src/resource_guard.h: 修复 pcap/zfex 头文件包含
- src/radiotap_template.h: 独立定义结构，移除 tx.hpp 依赖
- src/packet_queue.h: 添加 TxPacket 结构
- src/guard_interval.h: 添加 kGuardIntervalMs 常量
- Makefile: 更新 wfb_core 目标

#### Gap-02: pcap 句柄为 nullptr
**问题：** wfb_core.cpp:167,195,301 传递 `nullptr` 给线程，导致无法实际工作
**修复：**
1. 在 main() 中创建实际 pcap 句柄
2. 使用 `pcap_open_live()` 初始化接口
3. 将句柄传递给 scheduler_thread 和 tx_thread
**验证：** wfb_core 启动时不崩溃
**行数：** 20-30 行
**时间：** 20 分钟

---

### 🔴 P0: 调度器核心功能缺失

#### Gap-03: 调度器核心逻辑实现
**问题：** scheduler_worker.cpp 使用硬编码 node_id=1，未实现实际调度逻辑
**子任务：**
- **3a:** 调用 `get_next_node_to_serve()` 获取下一个节点（替代硬编码）
- **3b:** 实现 Token 三连发发射逻辑（3次重复发送，间隔 1ms）
- **3c:** 实现节点状态机跃迁（Active/Sleeping/Idle 三态）
- **3d:** 在节点切换时插入 5ms 保护间隔（apply_guard_interval）
**修复：**
1. 移除硬编码 node_id=1
2. 使用 scheduler.get_node_status() 获取实际节点状态
3. 实现 send_token_triple() 调用 pcap_inject 3 次
4. 在 switch_node() 中调用 usleep(5000) 实现保护间隔
**验证：** 调度器日志显示节点切换、Token 发送、保护间隔
**行数：** 80-100 行
**时间：** 60 分钟

---

### 🔴 P0: 安全关键问题

#### Gap-04: 命令注入风险
**问题：** tun_reader.cpp:168-173 使用 `popen()` 设置 txqueuelen，未验证设备名
**修复：**
1. 添加设备名校验函数 `validate_tun_name()`
2. 使用 `ioctl(SIOCSIFTXQLEN)` 替代 `popen()`
3. 或使用白名单验证设备名只包含 [a-zA-Z0-9_]
**验证：** 单元测试通过，安全扫描无警告
**行数：** 15-20 行
**时间：** 20 分钟

---

### 🟡 P1: 并发稳定性问题

#### Gap-05: 条件变量竞争条件
**问题：** threads.h:170-173 `clear_token()` 未通知条件变量
**修复：**
1. 在 `clear_token()` 中添加 `cv_send.notify_one()`
2. 确保在锁内通知
**验证：** tx_worker 单元测试无死锁
**行数：** 2 行
**时间：** 5 分钟

#### Gap-06: 空指针访问风险
**问题：** rx_demux.cpp:136 传递 nullptr 给 stats 后直接解引用
**修复：**
1. 在所有 stats-> 访问前添加空指针检查
2. 或使用默认统计对象
**验证：** 单元测试覆盖 stats=nullptr 场景
**行数：** 5-10 行
**时间：** 10 分钟

#### Gap-07: 整数溢出风险
**问题：** rx_demux.cpp:168 `token.duration_ms` (uint16_t) 与 `now_ms` (uint64_t) 相加可能溢出
**修复：**
1. 添加溢出检查
2. 限制最大 duration 为 60000ms
3. 使用 `std::min()` 限制值
**验证：** 边界值单元测试
**行数：** 5-8 行
**时间：** 10 分钟

#### Gap-08: 潜在死锁
**问题：** tx_worker.cpp:167-190 在持有锁时调用外部函数
**修复：**
1. 获取时间后释放锁
2. 在条件变量 wait 前解锁
3. 调用外部函数前确保无锁
**验证：** 死锁检测测试通过
**行数：** 10-15 行
**时间：** 15 分钟

#### Gap-09: 类型转换不安全
**问题：** scheduler_worker.cpp:106 将 uint32_t 直接转为 uint16_t
**修复：**
1. 添加范围检查 `if (time_quota_ms > 65535)`
2. 限制最大值为 65535
3. 添加警告日志
**验证：** 边界值单元测试
**行数：** 5 行
**时间：** 5 分钟

#### Gap-10: 未处理 stoi 异常
**问题：** config.cpp:42-54 使用 std::stoi() 未处理异常
**修复：**
1. 添加 try-catch 块
2. 捕获 `std::invalid_argument` 和 `std::out_of_range`
3. 返回友好错误信息
**验证：** 异常输入单元测试
**行数：** 15-20 行
**时间：** 15 分钟

---

### 🟡 P1: 配置与完善

#### Gap-11: 硬编码节点数配置化
**问题：** wfb_core.cpp:147-150 硬编码注册 10 个节点，应使用可配置参数
**修复：**
1. 在 Config 结构体添加 `node_count` 字段
2. 添加命令行参数 `--node-count`
3. 从配置文件或命令行参数获取节点数
4. 验证范围 1-255
**验证：** 配置测试，不同节点数启动
**行数：** 10-15 行
**时间：** 15 分钟

#### Gap-12: node_id 范围验证
**问题：** config.cpp 未完整验证 node_id 范围（1-255）
**修复：**
1. 在客户端模式验证中添加 `node_id > 255` 检查
2. 服务端模式验证 node_id > 0
**验证：** 边界值单元测试
**行数：** 3-5 行
**时间：** 5 分钟

---

## 执行顺序

```
Wave 1 (编译基础):
  Gap-01 (构造函数) -> Gap-10 (stoi异常，影响配置解析)
  目标: wfb_core 能够编译通过

Wave 2 (调度器核心):
  Gap-03a (调度器获取节点) -> Gap-03b (Token三连发)
  Gap-03c (状态机跃迁) -> Gap-03d (保护间隔)
  目标: 调度器核心逻辑完整

Wave 3 (集成):
  Gap-02 (pcap句柄) -> Gap-11 (节点数配置化)
  目标: 线程能够实际工作

Wave 4 (安全):
  Gap-04 (命令注入) -> Gap-12 (node_id验证)
  目标: 安全无漏洞

Wave 5 (稳定性):
  Gap-05 (竞争条件) -> Gap-06 (空指针)
  Gap-07 (溢出) -> Gap-08 (死锁)
  Gap-09 (类型转换)
  目标: 稳定无并发问题

Wave 6 (验证):
  编译测试 -> 单元测试 -> 集成测试完整运行
```

---

## 文件修改清单

| 文件 | 修改类型 | 缺口 |
|------|----------|------|
| `src/server_scheduler.h` | 修改 | Gap-01 |
| `src/wfb_core.cpp` | 修改 | Gap-01, Gap-02, Gap-11 |
| `src/scheduler_worker.cpp` | 修改 | Gap-03, Gap-09 |
| `src/tun_reader.cpp` | 修改 | Gap-04, Gap-07 |
| `src/threads.h` | 修改 | Gap-05 |
| `src/rx_demux.cpp` | 修改 | Gap-06, Gap-07 |
| `src/tx_worker.cpp` | 修改 | Gap-08 |
| `src/config.cpp` | 修改 | Gap-10, Gap-12 |
| `src/config.h` | 修改 | Gap-11 |

---

## 验证策略

### Wave 1 验证
```bash
make clean
make wfb_core
# 预期: 编译成功，无错误
```

### Wave 2-3 验证
```bash
./test_wfb_core
# 预期: 10/10 测试通过
```

### Wave 4 安全验证
```bash
# 测试命令注入防护
./test_tun_name_validation "test; rm -rf /"
# 预期: 验证失败，设备名被拒绝

./test_tun_name_validation "wfb0"
# 预期: 验证通过
```

### Wave 5 稳定性验证
```bash
# 死锁检测测试
./test_deadlock_detection
# 预期: 无死锁，条件变量正常工作

# 边界值测试
./test_boundary_values
# 预期: 溢出、类型转换边界正确处理
```

### Wave 6 完整验证
```bash
# 集成测试（完整运行）
sudo ./tests/integration_test_v2.sh
# 预期: 测试通过，无崩溃

# 调度器行为验证
./tests/test_scheduler_behavior.sh
# 预期: 节点切换日志显示，Token 发送 3 次

# 最终验证
make test
# 预期: 所有测试通过
```

---

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| pcap 句柄创建失败 | 中 | 高 | 添加错误处理和回退逻辑 |
| 调度器逻辑改动引入新 bug | 低 | 中 | 保留单元测试，逐步修改 |
| ioctl 调用权限问题 | 低 | 中 | 检查 root 权限，提供警告 |
| Token 三连发实现复杂 | 中 | 中 | 参考 Phase 1 TokenEmitter 实现 |

---

## 完成标准

1. [ ] `make wfb_core` 编译成功
2. [ ] 10/10 单元测试通过
3. [ ] 集成测试脚本完整运行成功
4. [ ] 代码审查问题全部修复
5. [ ] 验证报告中所有 "失败" 状态转为 "通过" 或 "部分"
6. [ ] Token 三连发功能验证
7. [ ] 节点切换保护间隔验证
8. [ ] 节点状态机跃迁验证

---

## 预估工作量（修订版）

| Wave | 任务 | 预估时间 |
|------|------|----------|
| Wave 1 | 编译阻塞修复 | 30 分钟 |
| Wave 2 | 调度器核心实现 | 60 分钟 |
| Wave 3 | pcap集成与配置 | 35 分钟 |
| Wave 4 | 安全修复 | 25 分钟 |
| Wave 5 | 稳定性修复 | 40 分钟 |
| Wave 6 | 完整验证 | 30 分钟 |
| **总计** | | **约 3.5 小时** |

---

## 验证报告修复追踪

| 验证真理 | 原状态 | 修复缺口 | 目标状态 |
|----------|--------|----------|----------|
| #1 wfb_core启动 | ✗ 失败 | Gap-01 | ✓ 通过 |
| #6 RX线程接收 | ✗ 失败 | Gap-02 | ✓ 通过 |
| #21 调度器运行 | ✗ 失败 | Gap-03a | ✓ 通过 |
| #22 Token三连发 | ✗ 失败 | Gap-03b | ✓ 通过 |
| #23 保护间隔 | ✗ 失败 | Gap-03d | ✓ 通过 |
| #26 状态机跃迁 | ✗ 失败 | Gap-03c | ✓ 通过 |
| #39 集成测试 | ✗ 失败 | 全部 | ✓ 通过 |

---

## 代码审查修复追踪

| 审查编号 | 级别 | 修复缺口 |
|----------|------|----------|
| CR-01 | Critical | Gap-04 |
| WR-01 | Warning | Gap-05 |
| WR-02 | Warning | Gap-06 |
| WR-03 | Warning | Gap-07 |
| WR-04 | Warning | Gap-08 |
| WR-05 | Warning | Gap-09 |
| WR-06 | Warning | Gap-10 |
| IN-04 | Info | Gap-11 |
| IN-05 | Info | Gap-12 |

---

*计划修订: 2026-04-28*
*版本: 2.0*
