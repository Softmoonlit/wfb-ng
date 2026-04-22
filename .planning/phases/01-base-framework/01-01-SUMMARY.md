---
phase: 01-base-framework
plan: 01
subsystem: radiotap
tags: [radiotap, template, mcs, dual-template, zero-latency-switching]

# Dependency graph
requires: []
provides:
  - Radiotap 双模板定义（控制帧 + 数据帧）
  - 模板初始化函数（init_radiotap_templates）
  - 零延迟模板切换函数（get_radiotap_template）
affects: [phase-02, phase-03]

# Tech tracking
tech-stack:
  added: []
  patterns:
    - 双模板分流模式：控制帧固定最低 MCS，数据帧动态高阶 MCS
    - 指针切换模式：通过指针返回模板，避免拷贝开销

key-files:
  created:
    - src/radiotap_template.h: Radiotap 双模板声明
    - src/radiotap_template.cpp: 模板初始化和切换实现
    - tests/test_radiotap_template.cpp: 完整单元测试（Linux 环境）
    - tests/test_radiotap_template_standalone.cpp: 独立测试（跨平台验证）
  modified: []

key-decisions:
  - "控制帧模板固定使用 MCS 0 + 20MHz（理论速率 6Mbps），确保全网覆盖"
  - "数据帧模板启用 short GI 提高吞吐量"
  - "通过指针切换模板，实现零延迟"

patterns-established:
  - "双模板分流：控制帧使用最低 MCS 确保可靠性，数据帧使用高阶 MCS 提高吞吐量"
  - "指针切换：避免模板拷贝开销，实现零延迟"

requirements-completed: [CTRL-04, CTRL-08]

# Metrics
duration: 4min
completed: 2026-04-22
---

# Phase 1 Plan 01: Radiotap 双模板分流系统 Summary

**实现 Radiotap 双模板分流系统，控制帧固定 MCS 0 确保全网覆盖，数据帧动态高阶 MCS 提高吞吐量，通过指针实现零延迟切换**

## Performance

- **Duration:** 4 min
- **Started:** 2026-04-22T05:38:18Z
- **Completed:** 2026-04-22T05:41:56Z
- **Tasks:** 3
- **Files modified:** 4

## Accomplishments
- 定义控制帧和数据帧双模板全局变量
- 实现模板初始化函数，控制帧固定 MCS 0 + 20MHz（理论速率 6Mbps）
- 实现零延迟模板切换函数，通过指针返回对应模板
- 编写单元测试验证模板正确性
- 创建独立测试版本用于跨平台验证

## Task Commits

每个任务作为 TDD 流程提交：

1. **Task 1: 定义 Radiotap 双模板全局变量** - 包含在 `deeac49` (feat)
2. **Task 2: 实现 Radiotap 模板初始化和切换函数** - 包含在 `deeac49` (feat)
3. **Task 3: 编写 Radiotap 模板单元测试** - 包含在 `deeac49` (test)

**Plan metadata:** `deeac49` (feat: Radiotap 双模板分流系统)

_Note: 由于任务紧密耦合，作为一个完整的 TDD 功能提交_

## Files Created/Modified
- `src/radiotap_template.h` - Radiotap 双模板声明，包含全局变量和函数声明
- `src/radiotap_template.cpp` - 模板初始化和切换函数实现，复用 init_radiotap_header()
- `tests/test_radiotap_template.cpp` - 完整单元测试，验证控制帧和数据帧模板正确性
- `tests/test_radiotap_template_standalone.cpp` - 独立测试版本，用于跨平台验证核心逻辑

## Decisions Made
- 控制帧模板固定使用 MCS 0 + 20MHz（理论速率 6Mbps），确保全网覆盖
- 数据帧模板启用 short GI 提高吞吐量
- 通过指针切换模板，避免拷贝开销，实现零延迟
- 创建独立测试版本，在 Windows 环境中验证核心逻辑，完整测试需要在 Linux 环境中运行

## Deviations from Plan

### 环境适配

**1. [Rule 3 - Blocking] 创建独立测试版本以适配 Windows 开发环境**
- **Found during:** Task 3（编写单元测试）
- **Issue:** 计划中的测试依赖 Linux 系统头文件（sys/socket.h, poll.h），在 Windows MSYS2 环境中无法编译
- **Fix:** 创建独立测试版本（test_radiotap_template_standalone.cpp），mock 必要的结构体和函数，验证核心逻辑正确性
- **Files modified:** tests/test_radiotap_template_standalone.cpp
- **Verification:** 独立测试通过，所有 PASS 输出
- **Committed in:** deeac49（Task 3 提交）

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** 创建独立测试版本用于跨平台验证，不影响功能实现。完整测试需要在 Linux 环境中运行（依赖 libpcap 和系统头文件）

## Issues Encountered
- Windows MSYS2 环境缺少 Linux 系统头文件（sys/socket.h, poll.h），无法直接编译依赖 tx.cpp 的测试
- 解决方案：创建独立测试版本，mock radiotap_header_t 结构和 init_radiotap_header 函数，验证核心逻辑
- 完整测试（test_radiotap_template.cpp）需要在 Linux 环境中编译运行

## User Setup Required
None - 无需外部服务配置。

## Next Phase Readiness
- Radiotap 双模板分流系统实现完成，控制帧和数据帧模板定义清晰
- 模板初始化和切换函数可用，为零延迟切换奠定基础
- 独立测试通过，验证核心逻辑正确性
- 下一步：实现全局序列号生成器（01-02-PLAN.md）

---
*Phase: 01-base-framework*
*Plan: 01*
*Completed: 2026-04-22*
