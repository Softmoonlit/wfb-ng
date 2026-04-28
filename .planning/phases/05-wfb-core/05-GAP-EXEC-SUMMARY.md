---
phase: 05
type: gap_closure_execution
started: 2026-04-28
status: in_progress
---

# Phase 5: Gap Closure 执行摘要

## 执行状态

**启动时间:** 2026-04-28  
**当前状态:** 进行中  
**已修复缺口:** 1/12 (Gap-01)  
**编译状态:** ✅ 成功

---

## 已完成的修复

### Gap-01: ServerScheduler 构造函数不匹配 ✅

**问题:** src/wfb_core.cpp:143 调用 `ServerScheduler scheduler(config.mcs, config.bandwidth)`，但构造函数不存在

**修复内容:**

1. **src/server_scheduler.h** - 添加双参数构造函数
   ```cpp
   ServerScheduler(int mcs, int bandwidth)
       : mcs_(mcs), bandwidth_(bandwidth) {}
   ```

2. **src/resource_guard.h** - 修复头文件包含
   - 添加 `#include <pcap/pcap.h>`
   - 添加 zfex 前向声明

3. **src/radiotap_template.h** - 独立定义结构
   - 移除 `#include "tx.hpp"` 依赖
   - 独立定义 `radiotap_header_t` 结构
   - 添加 `RadiotapTemplate` 类内联实现

4. **src/packet_queue.h** - 添加 TxPacket 定义
   ```cpp
   struct TxPacket {
       std::vector<uint8_t> data;
       size_t len = 0;
       uint64_t timestamp_us = 0;
   };
   ```

5. **src/guard_interval.h** - 添加常量
   ```cpp
   constexpr uint16_t kGuardIntervalMs = 5;
   ```

6. **Makefile** - 更新构建目标
   - 添加所有必要的源文件到 WFB_CORE_OBJS

**验证:**
```bash
$ make wfb_core
# 编译成功，仅 warnings（未使用变量）

$ ./wfb_core --help
用法: ./wfb_core [选项]
...
# 正常运行
```

**提交建议:**
```bash
git add -A
git commit -m "fix(05-gap-01): 修复 ServerScheduler 构造函数和编译问题

- 添加 ServerScheduler(int mcs, int bandwidth) 构造函数
- 修复 resource_guard.h 头文件包含
- 独立 radiotap_template.h 定义，移除 tx.hpp 依赖
- 添加 TxPacket 结构到 packet_queue.h
- 添加 kGuardIntervalMs 常量
- 更新 Makefile 包含所有必要源文件

编译: make wfb_core 成功"
```

---

## 待修复缺口

| 缺口 | 优先级 | 问题 | 状态 |
|------|--------|------|------|
| Gap-02 | P0 | pcap 句柄为 nullptr | ⏳ 待修复 |
| Gap-03 | P0 | 调度器使用存根实现 | ⏳ 待修复 |
| Gap-04 | P0 | 命令注入风险 | ⏳ 待修复 |
| Gap-05 | P1 | 条件变量竞争条件 | ⏳ 待修复 |
| Gap-06 | P1 | 空指针访问风险 | ⏳ 待修复 |
| Gap-07 | P1 | 整数溢出风险 | ⏳ 待修复 |
| Gap-08 | P1 | 潜在死锁 | ⏳ 待修复 |
| Gap-09 | P1 | 类型转换不安全 | ⏳ 待修复 |
| Gap-10 | P1 | 未处理 stoi 异常 | ⏳ 待修复 |
| Gap-11 | P1 | 硬编码节点数 | ⏳ 待修复 |
| Gap-12 | P1 | node_id 范围验证 | ⏳ 待修复 |

---

## 关键路径

下一步修复顺序:

1. **Gap-02 (pcap 句柄)** - 使线程能够实际工作
2. **Gap-03 (调度器逻辑)** - 实现核心调度功能
3. **Gap-04 (命令注入)** - 修复安全风险
4. **Gap-05~10 (稳定性)** - 修复并发问题
5. **Gap-11~12 (完善)** - 配置化改进

---

## 风险与阻塞

**当前阻塞:** 无  
**技术债务:**
- server_scheduler.cpp 有未使用变量警告（duration_ms, next_window）
- tun_reader.cpp 有未使用函数警告（calculate_watermark）
- 这些可以在后续清理中处理

---

## 时间追踪

| 活动 | 预估 | 实际 |
|------|------|------|
| Gap-01 修复 | 30 分钟 | 45 分钟 |
| 编译调试 | - | 30 分钟 |
| **小计** | **30 分钟** | **75 分钟** |

---

## 下一步行动

1. 继续修复 **Gap-02**: 在 wfb_core.cpp 中创建实际 pcap 句柄
2. 使用 `pcap_open_live()` 初始化 WiFi 接口
3. 将 pcap 句柄传递给 scheduler_thread 和 tx_thread

---

*执行摘要更新: 2026-04-28*  
*执行者: Claude Code*  
*状态: 进行中*
