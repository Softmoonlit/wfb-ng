---
phase: 03-server-scheduler
plan: 04
subsystem: 调度引擎集成
tags: [idle-patrol, interleave-scheduling, SCHED-09, SCHED-10, SCHED-13, SCHED-14]
requires: [03-01, 03-02]
provides: [空闲巡逻模式, 混合交织调度, 调度引擎完整组装]
affects: [server_scheduler.h, server_scheduler.cpp, test_scheduler.cpp, test_aq_sq.cpp]
tech-stack:
  added: [std::pair return type, constexpr constants]
  patterns: [空闲巡逻模式, 混合交织调度, 保护间隔集成]
key-files:
  created: []
  modified:
    - src/server_scheduler.h
    - src/server_scheduler.cpp
    - tests/test_scheduler.cpp
    - tests/test_aq_sq.cpp
decisions:
  - 将空闲巡逻常量移至 public 区域以便测试访问
  - 使用 std::pair<uint8_t, uint16_t> 返回节点 ID 和授权时长
  - SQ 节点使用 MICRO_PROBE_DURATION_MS (15ms) 而非 current_window
metrics:
  duration: 15min
  completed_date: 2026-04-22
  tasks_completed: 4
  files_modified: 4
  tests_added: 4
---

# Phase 03 Plan 04: 空闲巡逻与混合交织调度集成 Summary

## 一句话总结

完成服务端调度引擎最后组装，实现空闲巡逻模式（100ms 间隔探询 SQ）和混合交织调度（每 4 次 AQ 穿插 1 次 SQ），集成高精度保护间隔。

## 任务完成情况

| 任务 | 名称 | 状态 | 提交 |
|------|------|------|------|
| 1 | 添加空闲巡逻常量和接口 | ✅ 完成 | e3c3a30 |
| 2 | 实现空闲巡逻和混合交织调度 | ✅ 完成 | 64f7479 |
| 3 | 扩展测试覆盖空闲巡逻和混合交织 | ✅ 完成 | 61dbd6f |
| 4 | 验证高精度定时和实时调度提权 | ✅ 完成 | 无代码修改 |

## 关键实现

### 1. 空闲巡逻常量（per SCHED-09）

```cpp
static constexpr uint16_t IDLE_PATROL_INTERVAL_MS = 100;  // 空闲巡逻间隔
static constexpr uint16_t MICRO_PROBE_DURATION_MS = 15;   // 微探询时长
static constexpr int INTERLEAVE_RATIO = 4;                // 混合交织比例
```

### 2. 调度接口

- `get_next_node_to_serve()`: 获取下一个要服务的节点，返回 `{node_id, duration_ms}`
  - AQ 节点使用 `current_window`
  - SQ 节点使用 `MICRO_PROBE_DURATION_MS` (15ms)

- `execute_scheduling_cycle()`: 执行调度周期，集成保护间隔

- `update_node_state()`: 更新节点状态

### 3. 测试覆盖

新增 4 个测试用例：
- `test_idle_patrol_constants`: 验证常量值
- `test_get_next_node_to_serve`: 测试 AQ 节点选择
- `test_micro_probe_for_sq`: 测试 SQ 微探询时长
- `test_interleave_integration`: 测试 AQ/SQ 交织比例

## 验证结果

### test_scheduler.cpp

```
=== ServerScheduler 三态状态机测试 ===
[PASS] test_state_machine_initial_state
[PASS] test_state_iii_additive_increase
[PASS] test_state_ii_demand_fitting
[PASS] test_watchdog_protection
[PASS] test_max_window_dynamic
[PASS] test_state_transition_sequence
[PASS] test_aq_sq_migration
[PASS] test_idle_patrol_constants
[PASS] test_get_next_node_to_serve
[PASS] test_micro_probe_for_sq

=== 所有测试通过 ===
```

### test_aq_sq.cpp

```
=== AQ/SQ Manager Tests ===
[PASS] test_init_node
[PASS] test_migrate_to_aq
[PASS] test_migrate_to_sq
[PASS] test_migrate_idempotent
[PASS] test_get_next_node_empty
[PASS] test_get_next_node_idle_patrol
[PASS] test_get_next_node_interleave
[PASS] test_node_rotation
[PASS] test_interleave_integration
=== All tests passed ===
```

### 高精度定时和实时调度验证

- `kControlThreadPriority = 99` (控制面)
- `kTxThreadPriority = 90` (发射端)
- `kGuardIntervalMs = 5` (保护间隔)
- `apply_guard_interval()` 精度 < 100μs
- `set_thread_realtime_priority()` 实现正确
- `check_root_permission()` 实现正确

## 需求映射

| 需求 | 描述 | 状态 |
|------|------|------|
| SCHED-09 | 空闲巡逻模式（100ms 间隔探询单节点） | ✅ 完成 |
| SCHED-10 | 混合交织调度（每 N 次 AQ 穿插 1 次 SQ） | ✅ 完成 |
| SCHED-13 | 高精度定时（timerfd + 短自旋） | ✅ 验证通过 |
| SCHED-14 | 实时调度提权（控制面 99，发射端 90） | ✅ 验证通过 |

## Deviations from Plan

None - plan executed exactly as written.

## Self-Check: PASSED

- [x] 所有源文件已创建/修改
- [x] 所有提交已创建
- [x] 所有测试通过
- [x] 常量值正确
- [x] 调度逻辑正确

## Commits

| Hash | Message |
|------|---------|
| e3c3a30 | feat(03-04): add idle patrol constants and scheduling interfaces |
| 64f7479 | feat(03-04): implement idle patrol and interleave scheduling |
| 61dbd6f | test(03-04): add tests for idle patrol and interleave scheduling |

## 下一步

Phase 03 服务端调度引擎计划全部完成，准备运行 `/gsd-verify-phase 03` 验证阶段成果。
