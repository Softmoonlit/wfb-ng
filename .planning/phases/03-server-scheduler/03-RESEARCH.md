# Phase 03: 服务端调度引擎 - 研究

**研究日期**: 2026-04-22
**领域**: 实时调度算法、状态机设计、高精度定时
**置信度**: HIGH

## Summary

Phase 03 核心目标是实现服务端三态状态机调度器，这是整个无碰撞传输架构的中枢神经。研究涵盖四个核心子系统：三态状态机引擎、AQ/SQ 双队列管理、混合交织调度算法、下行呼吸周期。关键技术挑战在于将 Phase 2 已建立的进程架构、环形队列、实时调度能力与调度逻辑深度集成，实现微秒级定时精度的 Token Passing 调度。

**主要建议**: 采用事件驱动的状态机设计模式，复用 Phase 2 的 guard_interval 高精度定时实现，将 ServerScheduler 扩展为完整的调度引擎。

## Architectural Responsibility Map

| 能力 | 主要层级 | 次要层级 | 理由 |
|------|----------|----------|------|
| 三态状态机计算 | API/Backend | — | 纯计算逻辑，无 I/O 依赖 |
| Token 生成与发射 | MAC/物理层 | — | 直接控制空口，需要实时调度 |
| AQ/SQ 队列管理 | API/Backend | — | 节点状态追踪与迁移 |
| 高精度定时 | MAC/物理层 | — | 硬件相关，timerfd 系统调用 |
| 呼吸周期控制 | API/Backend | MAC/物理层 | 上层决策，下层执行 |

## User Constraints (from CONTEXT.md)

> 无 CONTEXT.md 文件，使用阶段描述中的约束。

### Locked Decisions

| 决策 | 来源 |
|------|------|
| MIN_WINDOW = 50ms | SCHED-05 |
| MAX_WINDOW = 300ms (10节点) / 600ms (5节点) | SCHED-06 |
| STEP_UP = 100ms | SCHED-07 |
| ELASTIC_MARGIN = 50ms | SCHED-08 |
| GUARD_INTERVAL = 5ms | Phase 1 已实现 |
| IDLE_PATROL_INTERVAL = 100ms | SCHED-09 |
| INTERLEAVE_RATIO = 3-5 | SCHED-10 |
| 呼出周期 = 800ms, 吸气周期 = 200ms | SCHED-12 |
| NACK窗口 = 25ms/节点 | SCHED-12 |
| Watchdog 连续 3 次静默才判定断流 | SCHED-02 |

### Claude's Discretion

- 状态机类结构设计
- AQ/SQ 队列数据结构选择
- 与 Phase 2 架构的集成方式
- 测试策略设计

### Deferred Ideas (OUT OF SCOPE)

- 加权的混合交织调度 (SCHED-ADV-01)
- 预测性窗口分配 (SCHED-ADV-02)
- 自适应 INTERLEAVE_RATIO (SCHED-ADV-03)

## Phase Requirements

| ID | 描述 | 研究支持 |
|----|------|----------|
| SCHED-01 | 实现三态状态机引擎 | 三态状态机设计章节 |
| SCHED-02 | Watchdog 动量拦截 | Watchdog 保护机制章节 |
| SCHED-03 | 状态 II 需量拟合公式 | 三态状态机设计章节 |
| SCHED-04 | 状态 III 加法扩窗公式 | 三态状态机设计章节 |
| SCHED-05 | MIN_WINDOW = 50ms | 已锁定，无需研究 |
| SCHED-06 | MAX_WINDOW 配置 | 已锁定，无需研究 |
| SCHED-07 | STEP_UP = 100ms | 已锁定，无需研究 |
| SCHED-08 | ELASTIC_MARGIN = 50ms | 已锁定，无需研究 |
| SCHED-09 | 空闲巡逻模式 | 空闲巡逻章节 |
| SCHED-10 | 混合交织调度 | 混合交织调度章节 |
| SCHED-11 | AQ/SQ 双队列管理 | 双队列管理策略章节 |
| SCHED-12 | 下行呼吸周期 | 下行呼吸周期章节 |
| SCHED-13 | 高精度定时 | 高精度定时实现章节 |
| SCHED-14 | SCHED_FIFO 提权 | Phase 2 已实现，复用 |

## Standard Stack

### Core

| 库 | 版本 | 用途 | 为何标准化 |
|----|------|------|-----------|
| C++11 | — | 核心语言 | 项目技术栈要求，Phase 2 已采用 |
| pthread | POSIX | 实时调度 | SCHED_FIFO 提权必需 |
| timerfd | Linux Kernel | 高精度定时 | 微秒级精度，Phase 2 已验证 |

### Supporting

| 库 | 版本 | 用途 | 何时使用 |
|----|------|------|---------|
| std::map | C++11 | 节点状态存储 | NodeState 映射 |
| std::deque | C++11 | AQ/SQ 队列 | 节点 ID 队列管理 |
| std::mutex | C++11 | 线程安全 | 队列操作保护 |
| std::atomic | C++11 | 无锁变量 | can_send, token_expire_time_ms |

### Alternatives Considered

| 标准方案 | 可替代方案 | 权衡 |
|---------|-----------|------|
| std::deque | std::list | deque 内存连续性更好，缓存友好 |
| std::map | std::unordered_map | 节点数少（≤10），map 足够且有序 |
| timerfd + spin | clock_nanosleep | timerfd 可与 poll/epoll 集成 |

**安装**: 无需额外依赖，使用 C++11 标准库和 Linux 系统调用。

## Architecture Patterns

### System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        ServerScheduler 主循环                           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                      呼吸周期状态机                              │   │
│  │   呼出(800ms) ──→ 吸气(200ms) ──→ 呼出(800ms) ──→ ...          │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                    │                                    │
│         ┌──────────────────────────┼──────────────────────────┐        │
│         ▼                          ▼                          ▼        │
│  ┌─────────────┐          ┌─────────────┐          ┌─────────────┐    │
│  │ 三态状态机  │          │ AQ/SQ 队列  │          │ Token 发射  │    │
│  │   引擎      │←────────→│   管理器    │←────────→│   器        │    │
│  └─────────────┘          └─────────────┘          └─────────────┘    │
│         │                        │                        │           │
│         ▼                        ▼                        ▼           │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                     Phase 2 基础设施                            │   │
│  │  ThreadSafeQueue | condition_variable | timerfd | SCHED_FIFO   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                         ┌─────────────────┐
                         │   WiFi Monitor  │
                         │   Mode (空口)    │
                         └─────────────────┘
```

### Recommended Project Structure

```
src/
├── server_scheduler.h      # 调度器主类（扩展现有）
├── server_scheduler.cpp    # 调度器实现（扩展现有）
├── aq_sq_manager.h         # AQ/SQ 队列管理器（新增）
├── aq_sq_manager.cpp       # 队列管理器实现（新增）
├── breathing_cycle.h       # 呼吸周期状态机（新增）
├── breathing_cycle.cpp     # 呼吸周期实现（新增）
├── token_emitter.h         # Token 发射器（已有）
├── token_emitter.cpp       # Token 发射实现（已有）
├── guard_interval.h        # 高精度定时（已有）
├── guard_interval.cpp      # 高精度定时实现（已有）
└── threads.h               # 线程定义（已有）

tests/
├── test_scheduler.cpp      # 调度器测试（扩展现有）
├── test_aq_sq.cpp          # 队列管理测试（新增）
├── test_breathing.cpp      # 呼吸周期测试（新增）
└── test_integration.cpp    # 集成测试（新增）
```

### Pattern 1: 三态状态机

**描述**: 使用事件驱动模式，每次 actual_used 输入触发状态跃迁计算。

**何时使用**: 服务端收到客户端反馈后，计算下一轮授权窗口。

**示例**:

```cpp
// 来源: src/server_scheduler.cpp 已有实现
enum class NodeStateEnum { STATE_I, STATE_II, STATE_III };

struct NodeState {
    uint8_t silence_count = 0;      // Watchdog 计数器
    NodeStateEnum current_state = NodeStateEnum::STATE_I;
    uint16_t current_window = MIN_WINDOW;
};

uint16_t ServerScheduler::calculate_next_window(
    uint8_t node_id,
    uint16_t actual_used,
    uint16_t allocated) {

    auto& state = nodes[node_id];

    // 状态 I：极速枯竭与睡眠 (Zero-Payload Drop)
    if (actual_used < 5) {
        state.silence_count++;
        if (state.silence_count >= 3) {
            // 连续 3 次静默，判定断流
            state.current_state = NodeStateEnum::STATE_I;
            state.current_window = MIN_WINDOW;
            // 迁移到 SQ
            migrate_to_sq(node_id);
            return MIN_WINDOW;
        }
        // Watchdog 保护：保持原窗口
        return allocated;
    }

    // 有活动，重置静默计数
    state.silence_count = 0;

    // 状态 III：高负载贪突发 (Additive Increase)
    if (actual_used >= allocated) {
        state.current_state = NodeStateEnum::STATE_III;
        uint16_t target = allocated + STEP_UP;
        state.current_window = std::min(target, MAX_WINDOW);
        // 确保在 AQ 中
        ensure_in_aq(node_id);
        return state.current_window;
    }

    // 状态 II：需量拟合 (Demand-Fitting)
    // 5ms <= actual_used < allocated
    state.current_state = NodeStateEnum::STATE_II;
    uint16_t target = actual_used + ELASTIC_MARGIN;
    state.current_window = std::max(target, MIN_WINDOW);
    // 迁移到 AQ
    migrate_to_aq(node_id);
    return state.current_window;
}
```

### Pattern 2: AQ/SQ 双队列管理

**描述**: 使用双端队列管理活跃节点和睡眠节点，支持 O(1) 队首/队尾操作。

**何时使用**: 调度器选择下一个服务节点时。

**示例**:

```cpp
#include <deque>
#include <unordered_set>

class AqSqManager {
private:
    std::deque<uint8_t> aq;  // 活跃队列
    std::deque<uint8_t> sq;  // 睡眠队列
    std::unordered_set<uint8_t> in_aq_set;  // 快速查找
    std::unordered_set<uint8_t> in_sq_set;
    std::mutex mtx;

public:
    // 从 AQ 获取下一个节点（轮询）
    uint8_t pop_from_aq() {
        std::lock_guard<std::mutex> lock(mtx);
        if (aq.empty()) return 0xFF;  // 无效节点

        uint8_t node = aq.front();
        aq.pop_front();
        aq.push_back(node);  // 轮询：放回队尾
        return node;
    }

    // 迁移节点到 AQ
    void migrate_to_aq(uint8_t node_id) {
        std::lock_guard<std::mutex> lock(mtx);
        if (in_aq_set.count(node_id)) return;  // 已在 AQ

        // 从 SQ 移除
        if (in_sq_set.count(node_id)) {
            sq.erase(std::remove(sq.begin(), sq.end(), node_id), sq.end());
            in_sq_set.erase(node_id);
        }

        // 加入 AQ
        aq.push_back(node_id);
        in_aq_set.insert(node_id);
    }

    // 迁移节点到 SQ
    void migrate_to_sq(uint8_t node_id) {
        std::lock_guard<std::mutex> lock(mtx);
        if (in_sq_set.count(node_id)) return;  // 已在 SQ

        // 从 AQ 移除
        if (in_aq_set.count(node_id)) {
            aq.erase(std::remove(aq.begin(), aq.end(), node_id), aq.end());
            in_aq_set.erase(node_id);
        }

        // 加入 SQ
        sq.push_back(node_id);
        in_sq_set.insert(node_id);
    }

    // 混合交织调度：返回下一个要服务的节点
    // 返回值: <node_id, is_from_sq>
    std::pair<uint8_t, bool> get_next_node(int interleave_count) {
        std::lock_guard<std::mutex> lock(mtx);

        // 场景 1: 全员深睡，空闲巡逻
        if (aq.empty()) {
            if (sq.empty()) return {0xFF, false};
            uint8_t node = sq.front();
            sq.pop_front();
            sq.push_back(node);  // 轮询
            return {node, true};
        }

        // 场景 2: 混合调度
        // 每服务 N 次 AQ，穿插 1 次 SQ
        static int aq_served = 0;

        if (!sq.empty() && aq_served >= interleave_count) {
            // 服务 SQ
            aq_served = 0;
            uint8_t node = sq.front();
            sq.pop_front();
            sq.push_back(node);
            return {node, true};
        }

        // 服务 AQ
        aq_served++;
        uint8_t node = aq.front();
        aq.pop_front();
        aq.push_back(node);
        return {node, false};
    }
};
```

### Pattern 3: 下行呼吸周期

**描述**: 固定周期的状态切换，800ms 呼出 + 200ms 吸气。

**何时使用**: 服务端广播大模型文件时。

**示例**:

```cpp
#include "guard_interval.h"

class BreathingCycle {
public:
    enum class Phase { EXHALE, INHALE };

private:
    Phase current_phase = Phase::EXHALE;
    uint64_t phase_start_ms = 0;
    const uint16_t EXHALE_DURATION_MS = 800;
    const uint16_t INHALE_DURATION_MS = 200;
    const uint16_t NACK_WINDOW_MS = 25;
    const uint16_t GUARD_INTERVAL_MS = 5;
    size_t num_nodes = 10;

public:
    // 更新呼吸周期状态
    Phase update(uint64_t current_time_ms) {
        uint64_t elapsed = current_time_ms - phase_start_ms;

        if (current_phase == Phase::EXHALE && elapsed >= EXHALE_DURATION_MS) {
            // 呼出结束，切换到吸气
            current_phase = Phase::INHALE;
            phase_start_ms = current_time_ms;
            return Phase::INHALE;
        }
        else if (current_phase == Phase::INHALE && elapsed >= INHALE_DURATION_MS) {
            // 吸气结束，切换到呼出
            current_phase = Phase::EXHALE;
            phase_start_ms = current_time_ms;
            return Phase::EXHALE;
        }

        return current_phase;
    }

    // 吸气阶段：依次给每个客户端分配 NACK 窗口
    void execute_inhale_phase(AqSqManager& manager, TokenEmitter& emitter) {
        // 暂停下发 (can_send = false)
        // 依次分配 NACK 窗口
        for (size_t i = 0; i < num_nodes; i++) {
            uint8_t node_id = static_cast<uint8_t>(i + 1);
            emitter.send_token(node_id, NACK_WINDOW_MS);

            // 保护间隔
            apply_guard_interval(GUARD_INTERVAL_MS);
        }
    }
};
```

### Anti-Patterns to Avoid

- **固定周期探询风暴**: 不要每 50ms 轮询所有节点，应使用空闲巡逻 + 混合交织
- **硬编码 MAX_WINDOW**: 必须根据节点数动态计算 `(N-1) × MAX_WINDOW < GRTT`
- **无 Watchdog 保护**: 单次静默不立即降级，需连续 3 次才判定断流
- **usleep() 轮询**: 必须使用 condition_variable 实现零延迟唤醒

## Don't Hand-Roll

| 问题 | 不要自建 | 使用方案 | 理由 |
|------|---------|---------|------|
| 高精度定时 | usleep() 循环 | timerfd + 空转自旋 | Phase 2 已实现，精度 < 100μs |
| 线程同步 | 自旋锁 | std::mutex + condition_variable | 标准库优化，避免优先级反转 |
| 原子操作 | volatile | std::atomic | 内存序保证，编译器优化 |
| 节点队列 | 自定义链表 | std::deque | 内存连续，缓存友好 |
| Token 发射 | 手动构造帧 | TokenEmitter + 三连发 | Phase 1 已实现 |

**关键洞察**: Phase 2 已提供完整的基础设施（ThreadSafeQueue、guard_interval、实时调度），Phase 3 应专注于调度逻辑，而非重复造轮。

## Runtime State Inventory

> 本阶段为功能实现，不涉及重命名/迁移，此部分不适用。

## Common Pitfalls

### Pitfall 1: 固定周期探询风暴

**问题描述**: 每 50ms 轮询所有节点，导致频谱占用 100%，电池耗尽。

**发生原因**: 错误认为"频繁探询 = 低延迟"。

**防范措施**:
- 空闲巡逻模式：AQ 为空时，100ms 间隔探询 1 个节点
- 混合交织调度：每 N 次 AQ 服务穿插 1 次 SQ 探询

**预警信号**:
- 频谱占用 > 80%（全员深睡时）
- 客户端功耗异常高

### Pitfall 2: Watchdog 误判断流

**问题描述**: 单次 Token 丢失立即降级到 MIN_WINDOW，吞吐量骤降。

**发生原因**: 未考虑无线环境丢包。

**防范措施**:
- Watchdog 动量拦截：连续 3 次静默才判定断流
- 保持上一轮窗口不变

**预警信号**:
- 状态频繁在 III → I 间跃迁
- 日志显示窗口突然从 300ms 降到 50ms

### Pitfall 3: MAX_WINDOW 超过 UFTP 超时

**问题描述**: 10 节点场景，MAX_WINDOW = 500ms，队尾节点等待 4500ms > UFTP GRTT。

**发生原因**: 未验证 `(N-1) × MAX_WINDOW < GRTT`。

**防范措施**:
- 初始化时计算：`get_max_window(num_nodes)`
- 运行时验证：打印最大等待时间

**预警信号**:
- UFTP 频繁超时断连
- Wireshark 显示 UFTP FIN/RST

### Pitfall 4: 节点切换无保护间隔

**问题描述**: 新节点 Token 立即发射，与上一节点残余射频包碰撞。

**发生原因**: 未考虑硬件 TX Queue 延迟。

**防范措施**:
- 每次节点切换插入 5ms 保护间隔
- 使用 `apply_guard_interval(5)`

**预警信号**:
- 碰撞率 > 0%
- Wireshark 显示相邻 Token 间隔 < 5ms

## Code Examples

### 三态状态机完整实现

```cpp
// 来源: 扩展 src/server_scheduler.cpp
#include "server_scheduler.h"
#include "aq_sq_manager.h"

void ServerScheduler::init_node(uint8_t node_id) {
    nodes[node_id] = NodeState{};
    nodes[node_id].current_window = MIN_WINDOW;
    nodes[node_id].current_state = NodeStateEnum::STATE_I;
    nodes[node_id].silence_count = 0;
    // 初始化时放入 SQ
    aq_sq_manager->migrate_to_sq(node_id);
}

uint16_t ServerScheduler::calculate_next_window(
    uint8_t node_id,
    uint16_t actual_used,
    uint16_t allocated) {

    if (nodes.find(node_id) == nodes.end()) {
        init_node(node_id);
    }

    auto& state = nodes[node_id];

    // 状态 I：极速枯竭与睡眠 (Zero-Payload Drop)
    if (actual_used < 5) {
        state.silence_count++;

        if (state.silence_count >= 3) {
            // 连续 3 次静默，判定断流
            state.current_state = NodeStateEnum::STATE_I;
            state.current_window = MIN_WINDOW;
            aq_sq_manager->migrate_to_sq(node_id);
            return MIN_WINDOW;
        }

        // Watchdog 保护：保持原窗口
        return allocated;
    }

    // 有活动，重置静默计数
    state.silence_count = 0;

    // 状态 III：高负载贪突发 (Additive Increase)
    if (actual_used >= allocated) {
        state.current_state = NodeStateEnum::STATE_III;
        uint16_t target = allocated + STEP_UP;
        state.current_window = std::min(target, get_max_window(num_nodes));
        aq_sq_manager->migrate_to_aq(node_id);
        return state.current_window;
    }

    // 状态 II：需量拟合 (Demand-Fitting)
    state.current_state = NodeStateEnum::STATE_II;
    uint16_t target = actual_used + ELASTIC_MARGIN;
    state.current_window = std::max(target, MIN_WINDOW);
    aq_sq_manager->migrate_to_aq(node_id);
    return state.current_window;
}

uint16_t ServerScheduler::get_max_window(size_t num_nodes) {
    // 确保 (N-1) × MAX_WINDOW < UFTP GRTT (假设 5000ms)
    if (num_nodes <= 5) return 600;
    if (num_nodes <= 10) return 300;
    return 150;  // 更多节点进一步减小
}
```

### 高精度定时复用

```cpp
// 来源: src/guard_interval.cpp 已实现
// Phase 3 可直接调用
#include "guard_interval.h"

void ServerScheduler::execute_scheduling_cycle() {
    // 获取下一个要服务的节点
    auto [node_id, is_from_sq] = aq_sq_manager->get_next_node(INTERLEAVE_RATIO);

    if (node_id == 0xFF) return;  // 无节点

    // 计算授权窗口
    uint16_t duration_ms;
    if (is_from_sq) {
        duration_ms = MICRO_PROBE_DURATION;  // 15ms 微探询
    } else {
        duration_ms = nodes[node_id].current_window;
    }

    // 发射 Token（三连发）
    token_emitter->send_token_triple(transmitter, node_id, duration_ms);

    // 保护间隔
    apply_guard_interval(kGuardIntervalMs);
}
```

## State of the Art

| 旧方案 | 当前方案 | 变更时机 | 影响 |
|--------|---------|---------|------|
| 固定周期轮询 | 空闲巡逻 + 混合交织 | Phase 3 | 频谱占用从 100% 降到 < 15% |
| 硬编码水位线 | MCS 动态映射 | Phase 2 | 排队延迟可控 |
| usleep() 轮询 | condition_variable | Phase 2 | 唤醒延迟从 5ms 降到 < 100μs |
| 单队列调度 | AQ/SQ 双队列 | Phase 3 | 区分活跃/睡眠节点 |

**已弃用/过时**:
- `usleep()`: 使用 `condition_variable` 替代
- `volatile`: 使用 `std::atomic` 替代
- 硬编码 MAX_WINDOW: 使用动态计算

## Assumptions Log

| # | 假设 | 章节 | 错误风险 |
|---|------|------|---------|
| A1 | UFTP GRTT 默认为 5000ms | MAX_WINDOW 计算 | 需确认实际 UFTP 配置 |
| A2 | 节点数固定 ≤ 10 | 队列设计 | 若节点数增加需扩展 |
| A3 | 硬件 TX Queue 在 5ms 内排空 | 保护间隔 | 需实测验证 |

## Open Questions

1. **客户端如何反馈 actual_used 时间？**
   - 已知：需要在 Token 过期后客户端回传
   - 不清楚：反馈帧格式和时机
   - 建议：定义 NACK 帧格式，包含 actual_used 字段

2. **吸气阶段 NACK 窗口如何处理多节点并发？**
   - 已知：25ms/节点顺序分配
   - 不清楚：是否需要保护间隔
   - 建议：每个 NACK 窗口后插入 5ms 保护间隔

## Environment Availability

| 依赖 | 需求方 | 可用 | 版本 | 备选 |
|------|--------|------|------|------|
| g++ | 编译器 | ✓ | 11+ | clang++ |
| pthread | 实时调度 | ✓ | POSIX | — |
| timerfd | 高精度定时 | ✓ | Linux Kernel | clock_nanosleep |
| libpcap | Token 发射 | ✓ | 1.10+ | raw socket |
| Root 权限 | SCHED_FIFO | ✓ | — | CAP_SYS_NICE |

**无阻塞依赖**: 所有必需工具已就绪。

## Validation Architecture

### Test Framework

| 属性 | 值 |
|------|-----|
| 框架 | assert (简单测试) |
| 配置文件 | 无 — 见 Wave 0 |
| 快速运行命令 | `./tests/test_scheduler` |
| 完整套件命令 | `make test` |

### Phase Requirements → Test Map

| 需求ID | 行为 | 测试类型 | 自动化命令 | 文件存在? |
|--------|------|----------|-----------|-----------|
| SCHED-01 | 三态状态机跃迁 | unit | `./tests/test_scheduler` | ✓ 需扩展 |
| SCHED-02 | Watchdog 保护 | unit | `./tests/test_scheduler` | ✓ 需扩展 |
| SCHED-03 | 需量拟合公式 | unit | `./tests/test_scheduler` | ✓ 已有 |
| SCHED-04 | 加法扩窗公式 | unit | `./tests/test_scheduler` | ✓ 已有 |
| SCHED-09 | 空闲巡逻模式 | unit | `./tests/test_aq_sq` | ❌ Wave 0 |
| SCHED-10 | 混合交织调度 | unit | `./tests/test_aq_sq` | ❌ Wave 0 |
| SCHED-11 | AQ/SQ 双队列 | unit | `./tests/test_aq_sq` | ❌ Wave 0 |
| SCHED-12 | 下行呼吸周期 | unit | `./tests/test_breathing` | ❌ Wave 0 |
| SCHED-13 | 高精度定时 | unit | `./tests/test_guard_interval` | ✓ 已有 |
| SCHED-14 | SCHED_FIFO 提权 | unit | `./tests/test_threads` | ✓ 已有 |

### Sampling Rate

- **每次任务提交**: `./tests/test_scheduler && ./tests/test_aq_sq`
- **每波合并**: `make test`
- **阶段门控**: 全套测试通过后执行 `/gsd-verify-phase 03`

### Wave 0 Gaps

- [ ] `tests/test_aq_sq.cpp` — 覆盖 SCHED-09, SCHED-10, SCHED-11
- [ ] `tests/test_breathing.cpp` — 覆盖 SCHED-12
- [ ] `src/aq_sq_manager.h` — AQ/SQ 管理器接口
- [ ] `src/aq_sq_manager.cpp` — 队列管理实现
- [ ] `src/breathing_cycle.h` — 呼吸周期接口
- [ ] `src/breathing_cycle.cpp` — 呼吸周期实现

## Security Domain

> 本阶段为调度逻辑实现，无安全敏感操作。安全域不适用。

### Applicable ASVS Categories

| ASVS 类别 | 适用 | 标准控制 |
|-----------|------|---------|
| V2 认证 | 否 | — |
| V3 会话管理 | 否 | — |
| V4 访问控制 | 否 | — |
| V5 输入验证 | 是 | 节点 ID 范围检查 |
| V6 加密 | 否 | — |

### Known Threat Patterns

| 模式 | STRIDE | 标准缓解 |
|------|--------|---------|
| 节点 ID 注入 | Tampering | 范围检查 1-10 |
| Token 重放 | Replay | 序列号单调递增 |

## Sources

### Primary (HIGH confidence)

- `.planning/research/ARCHITECTURE.md` - 系统架构设计
- `.planning/research/PITFALLS.md` - 陷阱防范清单
- `src/server_scheduler.cpp` - 已有调度器骨架
- `src/guard_interval.cpp` - 高精度定时实现

### Secondary (MEDIUM confidence)

- `src/threads.h` - 线程架构定义
- `src/token_emitter.h` - Token 发射器
- `tests/test_scheduler.cpp` - 已有测试

### Tertiary (LOW confidence)

- 无

## Metadata

**置信度分析**:
- 标准栈: HIGH - 复用 Phase 2 已验证的技术
- 架构: HIGH - 已有详细设计文档
- 陷阱: HIGH - PITFALLS.md 完整记录

**研究日期**: 2026-04-22
**有效期至**: 30 天（架构稳定）
