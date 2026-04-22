---
phase: 03-server-scheduler
plan: 03
subsystem: 调度引擎
tags: [breathing-cycle, state-machine, time-division, SCHED-12]
requires: [guard-interval]
provides: [breathing-cycle-state-machine]
affects: [server-scheduler, token-emitter]
tech-stack:
  added: [C++11, CLOCK_MONOTONIC, timerfd]
  patterns: [state-machine, time-division-multiplexing]
key-files:
  created:
    - src/breathing_cycle.h
    - src/breathing_cycle.cpp
    - tests/test_breathing.cpp
  modified: []
decisions:
  - 使用 initialized 标志而非 phase_start_ms == 0 检查初始化状态，避免 time=0 边界问题
  - execute_inhale_phase() 保留框架实现，Token 发射器集成留待后续计划
metrics:
  duration: 15 minutes
  completed: "2026-04-22"
  tasks: 3
  files: 3
  tests: 7
---

# Phase 03 Plan 03: 下行呼吸周期 Summary

实现下行广播"呼吸"周期状态机，在呼出阶段全速下发模型，吸气阶段分配 NACK 回传窗口。

## 一句话总结

呼吸周期状态机实现完成，支持 800ms 呼出/200ms 吸气的精确相位切换，吸气阶段为每个节点分配 25ms NACK 窗口并插入 5ms 保护间隔。

## 完成的任务

### Task 1: 创建呼吸周期状态机接口

**文件**: `src/breathing_cycle.h`

**内容**:
- 定义 `BreathingPhase` 枚举（EXHALE/INHALE）
- 定义 `BreathingCycle` 类接口
- 时间常量：
  - EXHALE_DURATION_MS = 800
  - INHALE_DURATION_MS = 200
  - NACK_WINDOW_MS = 25
  - GUARD_INTERVAL_MS = 5
- 核心方法：update(), execute_inhale_phase(), get_current_phase()

**提交**: 3ad5f99

### Task 2: 实现呼吸周期状态机逻辑

**文件**: `src/breathing_cycle.cpp`

**内容**:
- `update()`: 根据时间流逝自动切换相位（EXHALE 800ms -> INHALE 200ms -> EXHALE）
- `execute_inhale_phase()`: 为每个节点分配 NACK 窗口，插入保护间隔
- 集成 `apply_guard_interval()` 实现高精度节点切换保护

**提交**: 1994740

### Task 3: 创建呼吸周期单元测试

**文件**: `tests/test_breathing.cpp`

**内容**:
- `test_initial_phase`: 验证初始相位为 EXHALE
- `test_exhale_to_inhale_transition`: 验证 800ms 后切换到 INHALE
- `test_inhale_to_exhale_transition`: 验证 200ms 后切换回 EXHALE
- `test_cycle_duration`: 验证周期总时长 1000ms
- `test_full_cycle`: 验证完整呼吸周期
- `test_inhale_nack_windows`: 验证吸气阶段 NACK 窗口执行
- `test_set_num_nodes`: 验证节点数量设置和边界值检查

**结果**: 7/7 测试通过

**提交**: b077d6c

## 偏离计划的情况

### 自动修复的问题

**1. [Rule 1 - Bug] 修复 update() 初始化逻辑边界问题**

- **发现问题**: Task 3 测试执行期间
- **问题描述**: 使用 `phase_start_ms == 0` 检查初始化状态时，当 `current_time_ms` 为 0 时会导致初始化逻辑失败，相位切换异常
- **修复方案**: 添加 `bool initialized` 标志位，使用独立标志追踪初始化状态
- **修改文件**: `src/breathing_cycle.h`, `src/breathing_cycle.cpp`
- **提交**: b077d6c

**2. [Rule 1 - Bug] 移除未使用的 iostream include**

- **发现问题**: IDE 诊断警告
- **问题描述**: `breathing_cycle.cpp` 包含了未使用的 `<iostream>` 头文件
- **修复方案**: 移除未使用的 include
- **修改文件**: `src/breathing_cycle.cpp`
- **提交**: b077d6c

## 技术决策

| 冭策 | 原因 | 结果 |
|------|------|------|
| 使用 initialized 标志而非 phase_start_ms == 0 检查 | 避免 time=0 边界问题，语义更清晰 | 测试全部通过 |
| execute_inhale_phase() 保留框架实现 | Token 发射器尚未集成，留待后续计划 | 接口完整，便于集成 |

## 集成点

- **guard_interval.h**: 使用 `apply_guard_interval()` 实现节点切换保护
- **threads.h**: 未来集成到发射线程主循环
- **token_emitter.h**: 未来在 execute_inhale_phase() 中调用 Token 发射

## 验证结果

### 编译验证

```bash
# 独立编译
g++ -std=c++11 -Wall -Wextra -c src/breathing_cycle.cpp -o /tmp/breathing_cycle.o -I/home/kilome/wfb-ng/src
# 通过，无警告

# 测试编译
g++ -std=c++11 tests/test_breathing.cpp src/breathing_cycle.cpp src/guard_interval.cpp -o /tmp/test_breathing -I/home/kilome/wfb-ng/src
# 通过
```

### 测试验证

```bash
./tmp/test_breathing
=== 呼吸周期状态机测试 ===
test_initial_phase: PASSED
test_exhale_to_inhale_transition: PASSED
test_inhale_to_exhale_transition: PASSED
test_cycle_duration: PASSED
test_full_cycle: PASSED
test_inhale_nack_windows: PASSED
test_set_num_nodes: PASSED
=== 所有测试通过 ===
```

## 需求映射

| 需求 ID | 描述 | 状态 |
|---------|------|------|
| SCHED-12 | 下行呼吸周期：呼出 800ms 下发模型，吸气 200ms 分配 NACK 窗口 | 已完成 |

## 后续工作

1. **Token 发射器集成**: 在 execute_inhale_phase() 中集成实际的 Token 发射逻辑
2. **线程集成**: 将 BreathingCycle 集成到发射线程主循环
3. **配置化**: 支持动态调整 EXHALE_DURATION_MS 和 INHALE_DURATION_MS

## 自检清单

- [x] 所有任务完成
- [x] 所有测试通过
- [x] 代码编译无警告
- [x] 偏离情况已记录
- [x] 需求映射已更新

## Self-Check: PASSED

**验证项**:
- src/breathing_cycle.h: FOUND
- src/breathing_cycle.cpp: FOUND
- tests/test_breathing.cpp: FOUND
- 03-03-SUMMARY.md: FOUND
- Commit 3ad5f99: FOUND
- Commit 1994740: FOUND
- Commit b077d6c: FOUND
