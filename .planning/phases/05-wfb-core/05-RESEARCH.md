# Phase 5: 单进程 wfb_core 架构 - 研究

**日期**: 2026-04-28
**状态**: 完成

## 研究目标

理解如何将现有的 wfb_tx / wfb_rx / wfb_tun 多进程架构整合为单进程 wfb_core，确保控制面与数据面在同一进程内闭环，避免 UDP 通信环路丢包问题。

## 现有架构分析

### 多进程架构问题

当前 wfb-ng 使用三个独立进程：
- `wfb_tx`: 负责从 UDP 端口读取数据并通过 WiFi 发射
- `wfb_rx`: 负责接收 WiFi 数据并写入 UDP 端口
- `wfb_tun`: 负责 TUN 设备读写和 UDP 转发

**问题**: 进程间通过 localhost UDP 通信，存在以下风险：
1. 数据包在环路中静默丢失（无反压机制）
2. 调度延迟不可控
3. 无法实施统一的实时调度策略

### 单进程架构设计

参考前置阶段已完成的实现：

1. **Phase 1**: TokenFrame 控制帧、序列号生成器、Radiotap 双模板、Token 三连发
2. **Phase 2**: 线程共享状态、环形队列、动态水位线、实时调度提权
3. **Phase 3**: 三态状态机、AQ/SQ 队列管理、空闲巡逻、混合交织调度
4. **Phase 4**: UFTP 配置、启动脚本、集成测试

### 关键整合点

#### 数据流整合

```
TUN 设备 (fd)
    ↓ (tun_reader_thread)
FEC 编码 (zfex)
    ↓
Packet Queue (环形队列)
    ↓ (tx_main_loop - 门控条件)
pcap_inject (WiFi 发射)
```

#### 控制流整合

```
WiFi 接收 (pcap)
    ↓ (rx_demux_thread - SCHED_FIFO 99)
Token 解析
    ↓
Shared State 更新 (can_send, token_expire_time_ms)
    ↓
条件变量通知 → tx_main_loop 唤醒
```

## 技术方案

### 线程架构

| 线程 | 优先级 | 职责 |
|------|--------|------|
| rx_demux_thread | SCHED_FIFO 99 | 接收 WiFi 帧，解析 Token，更新共享状态 |
| tx_main_loop | SCHED_FIFO 90 | 零轮询唤醒，门控控制，pcap_inject |
| tun_reader_thread | CFS (普通) | TUN 读取，FEC 编码，制造反压 |
| scheduler_thread | SCHED_FIFO 95 | 服务端调度器（仅 server 模式） |

### 共享状态

沿用 `src/threads.h` 中的 `ThreadSharedState`:

```cpp
struct ThreadSharedState {
    std::mutex mtx;
    std::condition_variable cv_send;
    bool can_send = false;
    uint64_t token_expire_time_ms = 0;
    PacketQueue packet_queue;
    uint32_t dynamic_queue_limit = 0;
    std::atomic<bool> shutdown{false};
};
```

### 命令行接口

保持与现有脚本兼容：

```bash
# Server 模式
wfb_core --mode server -i wlan0 -c 6 -m 0 --tun wfb0

# Client 模式
wfb_core --mode client -i wlan0 -c 6 -m 0 --tun wfb0 --node-id 1
```

### 依赖库

- libpcap (WiFi inject/monitor)
- libzfex (FEC 编码)
- pthread (线程)
- Linux TUN/TAP 驱动

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 实时线程死锁 | 严格锁定顺序，最小临界区 |
| TUN 读取阻塞 | 非阻塞 I/O 或独立线程 |
| pcap_inject 延迟 | SCHED_FIFO 90 确保及时发射 |
| 资源泄漏 | RAII 管理，信号处理优雅退出 |

## 研究结论

单进程架构在技术上可行，且已在 Phase 1-4 完成所有核心组件。Phase 5 的主要工作是：

1. **整合入口**: 重写 wfb_core.cpp，支持 server/client 双模式
2. **线程编排**: 正确启动和管理所有工作线程
3. **信号处理**: SIGINT/SIGTERM 优雅退出
4. **脚本更新**: server_start.sh / client_start.sh 调用 wfb_core
5. **向后兼容**: 保留原有命令行参数支持

---
*Phase 5 研究完成*
