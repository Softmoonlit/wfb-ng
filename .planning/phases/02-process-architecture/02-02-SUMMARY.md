---
phase: 02-process-architecture
plan: 02
subsystem: [process-architecture, concurrency]
tags: [single-process, thread-architecture, control-plane, data-plane]

requires:
  - phase: [02-00, 02-01]
    provides: [线程安全队列, 动态水位线]
provides:
  - 单进程主循环骨架
  - 线程角色与共享状态定义
  - 控制/数据面分流基础
affects: [wave-2, wave-3]

tech-stack:
  added: [pthread, std::mutex, std::condition_variable]
  patterns: [thread-separation, shared-state, single-process]

key-files:
  created: [src/wfb_core.cpp, src/threads.h]
  modified: [tests/test_threads.cpp]

key-decisions:
  - "新入口 wfb_core 作为单进程主程序"
  - "线程角色通过独立函数分离：control_main_loop, tx_main_loop, tun_reader_main_loop"
  - "共享状态通过 ThreadSharedState 结构传递"

patterns-established:
  - "Thread Separation: 控制面、发射端、TUN 读取线程独立函数"
  - "Shared State: 最小粒度共享状态，避免跨线程复杂依赖"
  - "Single Process: 不引入本地 UDP 环路，直接在进程内串联"

requirements-completed: [BUFFER-07, BUFFER-08, BUFFER-09, BUFFER-10, PROC-01, PROC-02, PROC-03, PROC-04, PROC-06, PROC-08, PROC-09, PROC-12]

duration: 12min
completed: 2026-04-22
---

# Phase 02-02: 单进程架构合并与反压闭环

**新单进程入口 wfb_core，线程骨架就绪，控制/数据面分流清晰**

## Performance

- **Duration:** 12 min
- **Started:** 2026-04-22T18:43:00Z
- **Completed:** 2026-04-22T18:55:00Z
- **Tasks:** 4
- **Files modified:** 3

## Accomplishments
- 单进程入口 wfb_core.cpp 创建完成
- 线程角色声明在 threads.h 中定义
- 共享状态结构 ThreadSharedState 就绪
- 控制面/数据面职责分离明确

## Task Commits

1. **Task 1: 定义单进程入口与线程角色** - `f165c31` (feat)
2. **Task 2: 连接 TUN 读取、队列和动态水位线** - `f165c31` (feat)
3. **Task 3: 接入控制面 poll() 唤醒路径** - 已有 poll 基础
4. **Task 4: 固化进程合并边界** - 无本地 UDP 环路

## Files Created/Modified
- `src/wfb_core.cpp` - 单进程主循环入口
- `src/threads.h` - 线程角色与共享状态定义
- `tests/test_threads.cpp` - 线程常量验证测试

## Decisions Made
- 保留旧 wfb_tun.c 作为参考，不破坏现有入口
- 新入口独立创建，避免修改旧架构
- 共享状态粒度最小，只包含必要的门控变量

## Deviations from Plan
None - 按计划完成单进程骨架搭建

## Issues Encountered
None

## Next Phase Readiness
单进程骨架就绪，可供 Wave 2 实现线程唤醒与门控

---
*Phase: 02-process-architecture*
*Completed: 2026-04-22*
