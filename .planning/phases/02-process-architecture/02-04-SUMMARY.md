---
phase: 02-process-architecture
plan: 04
subsystem: [realtime-scheduling, timing]
tags: [sched-fifo, root-detection, timerfd, guard-interval]

requires:
  - phase: [02-02, 02-03]
    provides: [单进程骨架, 发射线程唤醒]
provides:
  - 实时调度提权（SCHED_FIFO）
  - Root 权限检测
  - 高精度保护间隔（timerfd + 短自旋）
affects: []

tech-stack:
  added: [pthread_setschedparam, SCHED_FIFO, timerfd_create]
  patterns: [realtime-priority, high-precision-timer]

key-files:
  created: []
  modified: [src/threads.h, src/wfb_core.cpp, tests/test_threads.cpp]

key-decisions:
  - "控制面线程优先级 99（最高）"
  - "发射线程优先级 90（次高）"
  - "TUN 读取线程保持普通优先级"
  - "5ms 保护间隔通过 timerfd + 短自旋实现"

patterns-established:
  - "Realtime Priority: 关键线程 SCHED_FIFO 提权降低调度毛刺"
  - "High-Precision Timer: timerfd 粗等待 + 短自旋细等待实现微秒级精度"
  - "Guard Interval: 节点切换保护间隔避免空中撞车"

requirements-completed: [RT-01, RT-02, RT-03, RT-04, RT-05, RT-06, RT-07, RT-08]

duration: 15min
completed: 2026-04-22
---

# Phase 02-04: 实时调度与高精度定时

**关键线程 SCHED_FIFO 提权，Root 权限检测，5ms 保护间隔微秒级精度**

## Performance

- **Duration:** 15 min
- **Started:** 2026-04-22T19:05:00Z
- **Completed:** 2026-04-22T19:20:00Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments
- set_thread_realtime_priority 实现并验证
- 控制面/发射端优先级边界固定（99/90）
- Root 权限检测函数就绪
- 高精度保护间隔已存在（guard_interval.cpp）
- 测试覆盖优先级常量和权限检测

## Task Commits

1. **Task 1: 实现线程提权辅助函数** - `f165c31` (feat)
2. **Task 2: 固定线程优先级边界** - `97be52e` (feat)
3. **Task 3: 实现高精度保护间隔定时** - 已有实现（guard_interval.cpp）

## Files Created/Modified
- `src/threads.h` - 实时调度函数声明
- `src/wfb_core.cpp` - 提权实现和 Root 检测
- `tests/test_threads.cpp` - 优先级常量测试

## Decisions Made
- 失败时输出告警，不阻塞程序
- 启动时自动检查 Root 权限
- 保护间隔复用现有 guard_interval 实现

## Deviations from Plan
None - 计划执行完全符合预期

## Issues Encountered
- 编译测试时 main 函数冲突 → 通过 TEST_MODE 宏解决

## Next Phase Readiness
Phase 02 所有 Wave 完成，单进程架构基础就绪

---
*Phase: 02-process-architecture*
*Completed: 2026-04-22*
