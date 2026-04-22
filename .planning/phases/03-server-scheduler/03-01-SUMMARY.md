---
phase: 03-server-scheduler
plan: 01
subsystem: 调度器核心
tags: [queue-manager, aq-sq, interleave-scheduling, thread-safety]
requires: [SCHED-10, SCHED-11]
provides: [AQ/SQ 队列管理器, 混合交织调度]
affects: [服务端调度器, 节点状态管理]
tech-stack:
  added:
    - std::deque (活跃/睡眠队列)
    - std::unordered_set (O(1) 查找)
    - std::mutex/lock_guard (线程安全)
  patterns:
    - 轮询调度模式
    - 混合交织调度
key-files:
  created:
    - src/aq_sq_manager.h
    - src/aq_sq_manager.cpp
    - tests/test_aq_sq.cpp
decisions:
  - 使用 std::deque 而非 std::list（内存连续性更好）
  - 使用 unordered_set 辅助查找（O(1) 复杂度）
  - 默认混合交织比例为 4（介于 3-5 之间）
metrics:
  duration: 15分钟
  completed_date: 2026-04-22
  tasks_completed: 3
  files_created: 3
  test_cases: 8
  test_pass_rate: 100%
---

# Phase 03 Plan 01: AQ/SQ 队列管理器 Summary

## 概述

实现了 AQ（活跃队列）和 SQ（睡眠队列）双队列管理器，支持节点状态动态迁移和混合交织调度。该管理器是服务端调度引擎的核心组件，用于区分活跃节点和睡眠节点，实现高效的调度节点选择。

## 关键实现

### 数据结构设计

```cpp
class AqSqManager {
    std::deque<uint8_t> aq;           // 活跃队列
    std::deque<uint8_t> sq;           // 睡眠队列
    std::unordered_set<uint8_t> in_aq_set;  // O(1) 查找
    std::unordered_set<uint8_t> in_sq_set;  // O(1) 查找
    std::mutex mtx;                   // 线程安全保护
    int aq_served = 0;                // 混合交织计数器
};
```

### 核心方法

| 方法 | 功能 |
|------|------|
| `init_node(node_id)` | 初始化节点到睡眠队列 |
| `migrate_to_aq(node_id)` | 迁移节点到活跃队列（幂等） |
| `migrate_to_sq(node_id)` | 迁移节点到睡眠队列（幂等） |
| `get_next_node(interleave_ratio)` | 获取下一个要服务的节点（混合交织调度） |

### 混合交织调度策略

`get_next_node()` 方法实现了三场景调度：

1. **全空场景**：AQ 和 SQ 均为空 → 返回 `{INVALID_NODE, false}`
2. **空闲巡逻场景**：AQ 为空，SQ 不空 → 从 SQ 轮询取节点，返回 `{node_id, true}`
3. **混合交织场景**：
   - 维护计数器 `aq_served`
   - 每服务 `interleave_ratio` 次 AQ，穿插 1 次 SQ
   - 轮询实现：取出队首，放回队尾

### 线程安全

所有公有方法使用 `std::lock_guard<std::mutex>` 保护，确保多线程环境下的安全性。

## 测试覆盖

| 测试用例 | 覆盖场景 |
|---------|---------|
| `test_init_node` | 节点初始化到 SQ |
| `test_migrate_to_aq` | 节点从 SQ 迁移到 AQ |
| `test_migrate_to_sq` | 节点从 AQ 迁移到 SQ |
| `test_migrate_idempotent` | 迁移操作幂等性 |
| `test_get_next_node_empty` | 空队列返回 INVALID_NODE |
| `test_get_next_node_idle_patrol` | 空闲巡逻模式 |
| `test_get_next_node_interleave` | 混合交织调度 |
| `test_node_rotation` | 轮询机制验证 |

**测试结果**: 8/8 通过，100% 通过率

## 需求映射

| 需求 ID | 描述 | 实现状态 |
|---------|------|---------|
| SCHED-10 | 混合交织调度（每 4 次 AQ 服务穿插 1 次 SQ） | ✅ 已实现 |
| SCHED-11 | AQ/SQ 队列管理，节点动态迁移 | ✅ 已实现 |

## 常量定义

```cpp
static constexpr uint8_t INVALID_NODE = 0xFF;  // 无效节点标识
static constexpr int DEFAULT_INTERLEAVE_RATIO = 4;  // 默认混合交织比例
```

## 与现有代码集成

该队列管理器将被 `ServerScheduler` 使用，用于：

1. 初始化新节点到睡眠队列
2. 根据节点活动状态迁移到活跃队列或睡眠队列
3. 通过 `get_next_node()` 选择下一个要服务的节点

## 提交记录

| Commit | 类型 | 描述 |
|--------|------|------|
| 000c21c | feat | 创建 AQ/SQ 队列管理器接口 |
| 0cad2d6 | feat | 实现 AQ/SQ 队列管理器核心逻辑 |
| 40fb2dd | test | 创建 AQ/SQ 队列管理器单元测试 |

## Self-Check

- [x] 文件 `src/aq_sq_manager.h` 存在
- [x] 文件 `src/aq_sq_manager.cpp` 存在
- [x] 文件 `tests/test_aq_sq.cpp` 存在
- [x] 所有测试通过（8/8）
- [x] 代码编译无错误

## 后续工作

- 集成到 `ServerScheduler` 中
- 与三态状态机（Plan 02）配合使用
- 与呼吸周期（Plan 03）配合使用

## Self-Check: PASSED

所有验证项已通过：
- src/aq_sq_manager.h: FOUND
- src/aq_sq_manager.cpp: FOUND
- tests/test_aq_sq.cpp: FOUND
- Commits: 000c21c, 0cad2d6, 40fb2dd - FOUND
