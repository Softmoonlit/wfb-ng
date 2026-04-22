# 架构

**Analysis Date:** 2026-04-22

## Pattern Overview

**Overall:** 三层架构分离与硬件级轮询调度 (TDMA-like)

**Key Characteristics:**
- 底层无连接裸包通信机制
- 基于Token的主从式时隙分配与带宽弹缩
- 高性能零拷贝/静态分配内存设计

## Layers

**应用层 (UFTP):**
- Purpose: 提供前向纠错 (FEC)、包分片/重组和乱序重排，主要承担视频或数传等高层业务逻辑
- Location: `src/zfex*`、`src/fec_test.cpp`
- Contains: 前向纠错编码实现、数据包恢复逻辑

**OS层 (TUN / 调度与队列管理):**
- Purpose: 拦截并缓冲系统网络包，负责线程安全的包队列管理及调度决策，控制发包速率和窗口大小
- Location: `src/server_scheduler.cpp`, `src/watermark.cpp`, `src/packet_queue.h`
- Contains: `ServerScheduler` 的三态弹性扩窗机制、动态水位线计算 (`calculate_dynamic_limit`)、安全环形队列 (`ThreadSafeQueue`)
- Depends on: 应用层数据、物理层反馈的传输速率

**物理层 (wfb-ng 魔改 / 裸 MAC 帧):**
- Purpose: 直接基于网卡的 Monitor 模式或 Raw socket 发送注入帧 (Injection)，包含控制帧结构的序列化与反序列化
- Location: `src/mac_token.cpp`, `src/mac_token.h`, `src/tx.cpp`, `src/rx.cpp`
- Contains: MAC 轮询 Token 生成与解析 (`serialize_token`, `parse_token`)，底层发送/接收处理

## Data Flow

**发送端下行流转:**

1. OS 层捕获数据流，存入高并发安全的物理内存池环形队列 (`src/packet_queue.h`)。
2. 调度器 (`src/server_scheduler.cpp`) 根据当前负载需求、上轮分配及物理层可用带宽 (`src/watermark.cpp`) 动态决定时隙窗口大小（三态弹性扩窗：极速枯竭与睡眠、需量拟合与退避、高负载贪突发）。
3. 生成调度 MAC Token (`src/mac_token.cpp`) 并结合业务数据序列化，最后由物理层裸发。

**State Management:**
- 队列状态由 `ThreadSafeQueue` 的互斥锁保护。
- 节点调度状态 (如连续静默次数 `silence_count` 和活跃标记 `in_aq`) 保存在 `ServerScheduler::nodes` 数组中。

## Key Abstractions

**动态水位线 (Dynamic Watermark):**
- Purpose: 容忍调度延迟，保障物理层满载运行的缓冲计算机制。
- Examples: `src/watermark.cpp` (`calculate_dynamic_limit`)
- Pattern: 根据底层 PHY Rate (bps)、分配时长 (ms) 和 MTU 动态计算发送帧数上限（通常为所需容量的 1.5 倍）。

**三态弹性扩窗 (Tri-state Elastic Windowing):**
- Purpose: 服务器端动态分配给各个节点 (Client) 的发包时隙/窗口。
- Examples: `src/server_scheduler.cpp` (`calculate_next_window`)
- Pattern: 根据节点的实际利用率进行调整：零载荷静默 (睡眠防抖)、按需拟合 (退避减小)、超出配额的激进扩展。

**MAC 控制令牌 (MAC Token):**
- Purpose: 控制节点收发时序的底层控制帧结构。
- Examples: `src/mac_token.cpp`
- Pattern: 固定结构体直接内存拷贝（序列化与反序列化）。

## Entry Points

**RX (接收侧):**
- Location: `src/rx.cpp`
- Triggers: 网卡捕获底层无线帧。
- Responsibilities: 裸包过滤、Token 解析及上层数据重组。

**TX (发送侧):**
- Location: `src/tx.cpp`
- Triggers: 调度器发出的时隙授权与本地队列非空事件。
- Responsibilities: 将业务数据打包和 Token 一并注入无线网卡发送。

## Error Handling

**Strategy:** 丢弃与快速恢复

**Patterns:**
- 队列满载处理：环形队列满时直接丢弃 (`src/packet_queue.h`)，防止内存溢出。
- FEC恢复：应用层利用前向纠错 (`src/zfex*`) 容忍物理层不可靠丢包。

## Cross-Cutting Concerns

**内存分配:** 彻底避免动态分配碎片，采用静态分配或初始化定长的物理内存池（如 `ThreadSafeQueue` 中提前 `resize`）。

---

*Architecture analysis: 2026-04-22*