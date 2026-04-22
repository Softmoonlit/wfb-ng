---
phase: 03-server-scheduler
plan: 02
subsystem: 调度器
tags: [state-machine, watchdog, dynamic-window, SCHED-01, SCHED-02, SCHED-03, SCHED-04, SCHED-06]
requires: [03-01]
provides: [三态状态机, Watchdog保护, 动态MAX_WINDOW]
affects: [server_scheduler.h, server_scheduler.cpp, test_scheduler.cpp]
tech-stack:
  added: [NodeStateEnum, 动态窗口计算]
  patterns: [三态状态机, Watchdog保护模式]
key-files:
  created: []
  modified:
    - src/server_scheduler.h
    - src/server_scheduler.cpp
    - tests/test_scheduler.cpp
decisions:
  - 使用枚举类 NodeStateEnum 定义三态状态
  - MAX_WINDOW 动态计算基于节点数量
  - Watchdog 保护需要连续 3 次静默才判定断流
metrics:
  duration: 15分钟
  tasks: 3
  files: 3
  tests: 7
  test_pass_rate: 100%
---

# Phase 03 Plan 02: 三态状态机扩展与 Watchdog 保护 Summary

## 一句话概述

扩展服务端三态状态机，实现完整的状态跃迁逻辑、Watchdog 保护（连续 3 次静默才判定断流）和动态 MAX_WINDOW 计算。

## 实现详情

### Task 1: 扩展 NodeState 结构和状态枚举

**提交**: 93e4a7f

**修改内容**:
- 添加 `NodeStateEnum` 枚举类，定义 STATE_I/II/III 三种状态
- 扩展 `NodeState` 结构，添加 `current_state` 和 `current_window` 成员
- 添加 `AqSqManager*` 成员变量用于队列管理器集成
- 添加 `set_aq_sq_manager()`、`set_num_nodes()` 配置接口
- 添加 `get_max_window()` 动态计算接口声明
- 移除硬编码的 `MAX_WINDOW` 常量，改为动态计算

### Task 2: 实现三态状态机核心逻辑

**提交**: c975cf0

**修改内容**:
- 实现 `get_max_window()` 动态计算:
  - 5 节点及以下: 600ms
  - 10 节点及以下: 300ms
  - 更多节点: 150ms
- 实现 `set_aq_sq_manager()` 和 `set_num_nodes()` 配置方法
- 重写 `calculate_next_window()` 完整状态机逻辑:
  - **状态 I** (actual_used < 5ms): Watchdog 保护，连续 3 次静默才降级到 MIN_WINDOW
  - **状态 II** (5ms <= actual_used < allocated): 需量拟合，使用 `max(MIN, actual + MARGIN)`
  - **状态 III** (actual_used >= allocated): 加法扩窗，使用 `min(MAX, allocated + STEP)`
- 集成 AQ/SQ 队列迁移调用

### Task 3: 扩展状态机单元测试

**提交**: 1794052

**新增测试**:
1. `test_state_machine_initial_state` - 验证初始状态和窗口
2. `test_state_iii_additive_increase` - 验证状态 III 扩窗逻辑
3. `test_state_ii_demand_fitting` - 验证状态 II 需量拟合
4. `test_watchdog_protection` - 验证 Watchdog 3 次静默保护
5. `test_max_window_dynamic` - 验证动态 MAX_WINDOW 计算
6. `test_state_transition_sequence` - 验证完整状态跃迁序列
7. `test_aq_sq_migration` - 验证 AQ/SQ 队列迁移集成

**测试结果**: 7/7 通过 (100%)

## 关键设计决策

### 1. NodeStateEnum 枚举类设计

使用 `enum class` 而非普通枚举，提供更好的类型安全和作用域隔离：

```cpp
enum class NodeStateEnum {
    STATE_I,   // 枯竭与睡眠
    STATE_II,  // 唤醒与需量拟合
    STATE_III  // 高负载贪突发
};
```

### 2. 动态 MAX_WINDOW 计算

根据节点数量动态调整 MAX_WINDOW，确保满足 `(N-1) × MAX_WINDOW < UFTP GRTT` 约束：

| 节点数 | MAX_WINDOW | 计算验证 |
|--------|------------|----------|
| 5      | 600ms      | 4 × 600 = 2400ms < 5000ms |
| 10     | 300ms      | 9 × 300 = 2700ms < 5000ms |
| 20     | 150ms      | 19 × 150 = 2850ms < 5000ms |

### 3. Watchdog 保护机制

状态 I 的静默判定需要连续 3 次才触发降级：
- 防止单次丢包导致的误判
- 保持窗口稳定性
- 提供冗余保护

## 需求映射

| 需求 ID | 描述 | 状态 |
|---------|------|------|
| SCHED-01 | 三态状态机正确实现状态 I/II/III 跃迁 | 已实现 |
| SCHED-02 | Watchdog 保护，连续 3 次静默才判定断流 | 已实现 |
| SCHED-03 | 状态 II 使用 max(MIN, actual + MARGIN) 公式 | 已实现 |
| SCHED-04 | 状态 III 使用 min(MAX, allocated + STEP) 公式 | 已实现 |
| SCHED-05 | MIN_WINDOW = 50ms | 已实现 |
| SCHED-06 | MAX_WINDOW 根据节点数动态计算 | 已实现 |
| SCHED-07 | STEP_UP = 100ms | 已实现 |
| SCHED-08 | ELASTIC_MARGIN = 50ms | 已实现 |

## Deviations from Plan

无偏差 - 计划完全按预期执行。

## 验证结果

### 编译验证
```
make clean && make  # 成功
```

### 测试验证
```
=== ServerScheduler 三态状态机测试 ===
[PASS] test_state_machine_initial_state
[PASS] test_state_iii_additive_increase
[PASS] test_state_ii_demand_fitting
[PASS] test_watchdog_protection
[PASS] test_max_window_dynamic
[PASS] test_state_transition_sequence
[PASS] test_aq_sq_migration

=== 所有测试通过 ===
```

### 代码验证
- 状态枚举定义完整
- Watchdog 逻辑正确（silence_count >= 3）
- 公式实现正确（STEP_UP、ELASTIC_MARGIN）

## 提交记录

| 提交 | 类型 | 描述 |
|------|------|------|
| 93e4a7f | feat | 扩展 NodeState 结构和状态枚举 |
| c975cf0 | feat | 实现三态状态机核心逻辑 |
| 1794052 | test | 扩展状态机单元测试 |

## Self-Check: PASSED

- [x] src/server_scheduler.h 存在
- [x] src/server_scheduler.cpp 存在
- [x] tests/test_scheduler.cpp 存在
- [x] 03-02-SUMMARY.md 存在
- [x] 提交 93e4a7f 存在
- [x] 提交 c975cf0 存在
- [x] 提交 1794052 存在
- [x] 所有测试通过 (7/7)
