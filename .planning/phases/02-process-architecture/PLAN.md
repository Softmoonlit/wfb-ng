# Phase 2: 进程架构与实时调度 - 主计划

**阶段**: Phase 2 - 进程架构与实时调度
**创建日期**: 2026-04-22
**状态**: ✅ 验证通过，待执行

---

## 阶段目标

构建单进程架构，实现控制/数据面分流、线程安全环形队列、动态水位线与实时调度，消除 UDP 通信环路丢包和轮询延迟毛刺。

**核心价值**:
- 合并 wfb_tun 与 wfb_tx 为单进程主循环
- 建立线程安全环形队列，作为 TUN 与发射链路之间的缓冲层
- 接入动态水位线计算和 MCS 速率映射表
- 实现控制面 / 数据面分流、零轮询唤醒和快速反压
- 完成 SCHED_FIFO 提权与高精度定时基础

---

## 需求映射

| 需求 ID | 描述 | 计划 | 状态 |
|---------|------|------|------|
| BUFFER-01 | 实现线程安全的环形包队列 | 02-00 | ○ 待实现 |
| BUFFER-02 | 队列满时直接丢弃新包 | 02-00 | ○ 待实现 |
| BUFFER-03 | 实现动态逻辑水位线计算函数 | 02-01 | ○ 待实现 |
| BUFFER-04 | 实现 MCS 速率映射表 | 02-01 | ○ 待实现 |
| BUFFER-05 | 客户端水位线公式 | 02-01 | ○ 待实现 |
| BUFFER-06 | 服务端水位线公式 | 02-01 | ○ 待实现 |
| BUFFER-07 | 达到动态水位线时停止 TUN 读取 | 02-02 | ○ 待实现 |
| BUFFER-08 | 停止读网卡后触发内核队列溢出丢包 | 02-02 | ○ 待实现 |
| BUFFER-09 | TUN 初始化时设置 txqueuelen=100 | 02-02 | ○ 待实现 |
| BUFFER-10 | TUN 队列溢出时触发 ENOBUFS 反压 | 02-02 | ○ 待实现 |
| PROC-01 | 合并 wfb_tun 和 wfb_tx 为单进程 | 02-02 | ○ 待实现 |
| PROC-02 | 实现控制面/数据面分流高速路由 | 02-02 | ○ 待实现 |
| PROC-03 | 实现 TUN 读取线程 | 02-02 | ○ 待实现 |
| PROC-04 | 实现控制面接收线程 | 02-02 | ○ 待实现 |
| PROC-05 | 实现空口发射线程 | 02-03 | ○ 待实现 |
| PROC-06 | 控制面接收线程使用 poll() 替代 usleep() | 02-02 | ○ 待实现 |
| PROC-07 | 发射端线程使用 condition_variable 替代 usleep() | 02-03 | ○ 待实现 |
| PROC-08 | 实现 can_send / token_expire_time_ms / dynamic_queue_limit | 02-02 | ○ 待实现 |
| PROC-09 | TUN 读取线程检查 dynamic_queue_limit 并退避 | 02-02 | ○ 待实现 |
| PROC-10 | 发射端线程在 condition_variable 唤醒时立即释放锁 | 02-03 | ○ 待实现 |
| PROC-11 | 发射端线程检查 can_send 和 token_expire_time_ms | 02-03 | ○ 待实现 |
| PROC-12 | TUN 读取线程装弹后通知发射端 | 02-02 | ○ 待实现 |
| RT-01 | 实现 set_thread_realtime_priority() | 02-04 | ○ 待实现 |
| RT-02 | 控制面接收线程提权至 SCHED_FIFO 99 | 02-04 | ○ 待实现 |
| RT-03 | 空口发射线程提权至 SCHED_FIFO 90 | 02-04 | ○ 待实现 |
| RT-04 | TUN 读取线程保持普通优先级 | 02-04 | ○ 待实现 |
| RT-05 | 启动时检测 Root 权限 | 02-04 | ○ 待实现 |
| RT-06 | 使用 timerfd_create() 创建高精度定时器 | 02-04 | ○ 待实现 |
| RT-07 | 使用 clock_gettime() 获取纳秒级时间戳 | 02-04 | ○ 待实现 |
| RT-08 | 实现 5ms 保护间隔的高精度休眠 | 02-04 | ○ 待实现 |

---

## 成功标准

1. `ThreadSafeQueue` 静态分配 10000 包容量，线程安全，队列满时丢弃新包
2. 动态水位线计算正确，基于物理速率、时长和 MTU 输出正确 limit
3. MCS 速率映射表完整，能从启动参数解析出实时物理速率
4. TUN 读取线程在达到动态水位线时停止读取，并触发反压链路
5. 控制面接收线程使用 `poll()` 快速唤醒，不再依赖 `usleep()`
6. 发射端线程使用 `std::condition_variable` 零轮询唤醒
7. 三个线程优先级正确，`SCHED_FIFO` 策略生效
8. 高精度定时实现正确，5ms 保护间隔精度满足要求
9. 进程合并完成，无 UDP 通信环路，数据路径无静默丢失
10. TUN 队列 `txqueuelen = 100` 设置生效，快速丢包反压可触发

---

## 执行计划

### Wave 结构

```
Wave 0 (缓冲与水位线基础):
  ├─ 02-00-PLAN.md — 完善线程安全队列与队列语义
  └─ 02-01-PLAN.md — 建立动态水位线与 MCS 映射表

Wave 1 (进程合并与反压闭环):
  └─ 02-02-PLAN.md — 合并 wfb_tun / wfb_tx 并接通反压闭环

Wave 2 (发射线程与零轮询唤醒):
  └─ 02-03-PLAN.md — 实现发射端线程、can_send 阀门和条件变量唤醒

Wave 3 (实时调度与高精度定时):
  └─ 02-04-PLAN.md — 实现实时提权、Root 检查和 timerfd 休眠
```

### 计划详情

#### Wave 0: 缓冲与水位线基础

**02-00-PLAN.md** — 完善线程安全队列与队列语义
- 需求: BUFFER-01, BUFFER-02
- 文件: `src/packet_queue.h`, `tests/test_packet_queue.cpp`
- 任务: 3 个（确认容量语义、满队列丢弃、补充并发测试）
- 依赖: 无
- 状态: 待执行

**02-01-PLAN.md** — 建立动态水位线与 MCS 映射表
- 需求: BUFFER-03 ~ BUFFER-06
- 文件: `src/watermark.h`, `src/watermark.cpp`, `tests/test_watermark.cpp`
- 任务: 3 个（扩展 MCS 映射、计算 helper、补充测试）
- 依赖: 无
- 状态: 待执行

---

#### Wave 1: 进程合并与反压闭环

**02-02-PLAN.md** — 合并 wfb_tun / wfb_tx 并接通反压闭环
- 需求: BUFFER-07 ~ BUFFER-10, PROC-01 ~ PROC-04, PROC-06, PROC-08, PROC-09, PROC-12
- 文件: `src/wfb_core.cpp`, `src/threads.h`, `src/packet_queue.h`, `src/watermark.h`, `src/wfb_tun.c`
- 任务: 4 个（合并主循环、接入队列、水位线反压、控制面分流）
- 依赖: 02-00, 02-01
- 状态: 待执行

---

#### Wave 2: 发射线程与零轮询唤醒

**02-03-PLAN.md** — 实现发射端线程、can_send 阀门和条件变量唤醒
- 需求: PROC-05, PROC-07, PROC-10, PROC-11
- 文件: `src/threads.h`, `src/wfb_core.cpp`, `src/tx.hpp`, `src/tx.cpp`
- 任务: 3 个（发射线程骨架、条件变量唤醒、阀门检查）
- 依赖: 02-02
- 状态: 待执行

---

#### Wave 3: 实时调度与高精度定时

**02-04-PLAN.md** — 实现实时提权、Root 检查和 timerfd 休眠
- 需求: RT-01 ~ RT-08
- 文件: `src/threads.h`, `src/wfb_core.cpp`, `tests/test_threads.cpp`
- 任务: 3 个（提权函数、权限检查、高精度休眠）
- 依赖: 02-02, 02-03
- 状态: 待执行

---

## 关键决策

以下决策已在 CONTEXT 中确定，计划必须遵守：

### 队列与水位线（BUFFER-01 ~ BUFFER-06）
- **D-01**: 使用静态分配的 `ThreadSafeQueue` 作为环形包队列
- **D-02**: 队列满时直接丢弃新包，不做阻塞等待
- **D-03**: 水位线由 `calculate_dynamic_limit()` 统一计算，不允许硬编码
- **D-04**: MCS 速率映射必须内置到代码中，由启动参数选择

### 进程与线程架构（PROC-01 ~ PROC-12）
- **D-05**: 必须合并 `wfb_tun` 与 `wfb_tx`，禁止独立进程 UDP 环路
- **D-06**: 控制面与数据面在单进程内多线程分流
- **D-07**: 控制面接收线程使用 `poll()` 进行事件驱动唤醒
- **D-08**: 发射端线程使用 `std::condition_variable` 替代 `usleep()`
- **D-09**: `can_send`、`token_expire_time_ms`、`dynamic_queue_limit` 作为全局原子/共享状态
- **D-10**: TUN 线程在达到水位线后停止读取，靠内核队列形成反压

### 实时调度（RT-01 ~ RT-08）
- **D-11**: 调度关键线程必须使用 `SCHED_FIFO`
- **D-12**: 控制面线程优先级 99，发射线程优先级 90
- **D-13**: TUN 读取线程保持普通优先级，避免 CPU 密集路径实时化
- **D-14**: 启动时明确检查 Root 权限并打印告警
- **D-15**: 高精度休眠采用 `timerfd` + 极短时间自旋
- **D-16**: 时间戳统一使用 `clock_gettime()` 获取纳秒级基准

---

## 测试策略

### 单元测试
- 每个计划包含独立单元测试
- 测试命名：`tests/test_<module>.cpp`
- 覆盖率要求：> 80%

### 集成测试
- Phase 2 结束时执行端到端集成测试
- 验证流程：TUN 读取 → 队列入队 → 水位线反压 → 控制面唤醒 → 发射线程出队

### 性能测试
- 队列满丢弃语义测试
- 动态水位线映射测试
- 线程唤醒延迟测试
- 保护间隔精度测试

---

## 验收标准

### 代码质量
- 遵循 Google C++ Style Guide
- 所有函数和关键逻辑添加中文注释
- 通过 Cppcheck 和 clang-tidy 静态分析
- 使用 RAII 和智能指针，避免裸指针
- 多线程访问必须加锁或使用原子操作

### 测试覆盖
- 所有单元测试通过
- 测试覆盖率 > 80%
- 无内存泄漏

### 功能验证
- 满足所有 30 个需求（BUFFER-01 ~ BUFFER-10, PROC-01 ~ PROC-12, RT-01 ~ RT-08）
- 关键决策（D-01 ~ D-16）全部实现
- Phase 2 输出物全部存在并可被后续阶段复用

---

## 风险与缓解

### 技术风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 进程合并后引入线程死锁 | 中 | P1 | 拆分线程职责，使用超时与单向唤醒 |
| 动态水位线与实际速率偏差 | 中 | P1 | 先接通映射表和测试，再接主循环 |
| 实时提权权限不足 | 低 | P0 | 启动时检测并输出明确告警 |
| `condition_variable` 用法错误 | 低 | P1 | 先补测试，再接发射主循环 |

### 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 主循环改动面过大 | 中 | P1 | 按 Wave 顺序落地，先基础后合并 |
| 新入口与现有入口冲突 | 低 | P1 | 保留旧程序，新增 `wfb_core` 入口 |

---

## 执行命令

**开始执行**:
```bash
/gsd-execute-phase 02
```

**执行顺序**:
1. Wave 0: 02-00, 02-01
2. Wave 1: 02-02
3. Wave 2: 02-03
4. Wave 3: 02-04

**预计工作量**: 7-9 天

---

## 文档引用

- **上下文**: `.planning/phases/02-process-architecture/02-CONTEXT.md`
- **讨论日志**: `.planning/phases/02-process-architecture/02-DISCUSSION-LOG.md`
- **验证报告**: `.planning/phases/02-process-architecture/PLAN-CHECK.md`
- **项目文档**: `.planning/PROJECT.md`
- **需求文档**: `.planning/REQUIREMENTS.md`
- **路线图**: `.planning/ROADMAP.md`

---

*主计划创建: 2026-04-22*
*阶段: Phase 2 - 进程架构与实时调度*
*计划数: 4 个子计划*
*状态: ✅ 验证通过，待执行*
