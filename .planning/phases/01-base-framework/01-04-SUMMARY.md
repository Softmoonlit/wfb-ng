---
phase: 01-base-framework
plan: 04
subsystem: 控制平面
tags: [token-emitter, 三连发, 去重, bypass_fec, 序列号]

# Dependency graph
requires:
  - phase: 01-01
    provides: Radiotap 双模板分流系统
  - phase: 01-02
    provides: 全局单调递增序列号生成器
  - phase: 01-03
    provides: bypass_fec 参数实现
provides:
  - Token 三连发发射器（send_token_triple）
  - 接收端序列号去重（is_duplicate_token）
  - 三连发击穿机制实现
affects: [调度器, 节点通信, 控制帧传输]

# Tech tracking
tech-stack:
  added: [Token 发射器模块]
  patterns: [三连发击穿, 序列号去重, bypass_fec 快速路径]

key-files:
  created: [src/token_emitter.h, src/token_emitter.cpp, tests/test_token_emitter.cpp]
  modified: []

key-decisions:
  - "三连发使用相同序列号，接收端自动去重"
  - "三连发立即连续，无延迟间隔"
  - "使用 bypass_fec=true 跳过 FEC 编码"

patterns-established:
  - "三连发击穿：循环调用 3 次 send_packet，提高可靠性"
  - "序列号去重：seq_num <= last_seq 判定为重复包"

requirements-completed: [CTRL-06]

# Metrics
duration: 10min
completed: 2026-04-22
commits: 2
files_modified: 3
lines_added: 165
---

# Phase 1 Plan 04: Token 三连发发射器 Summary

**实现 Token 三连发发射器，三连发击穿提高可靠性，接收端通过序列号去重**

## Performance

- **Duration:** 10 分钟
- **Started:** 2026-04-22T05:55:06Z
- **Completed:** 2026-04-22T06:05:00Z
- **Tasks:** 3
- **Files modified:** 3

## Accomplishments

- Token 三连发发射器正确实现，连续调用 3 次 send_packet
- 三次发送使用相同的序列号，接收端通过序列号自动去重
- 使用 bypass_fec=true 跳过 FEC 编码，确保控制帧快速传输
- 接收端去重逻辑正确，通过序列号比较识别重复包

## Task Commits

每个任务已原子提交：

1. **Task 1 & 2 & 3: Token 发射器测试和实现** - `a4caf1a` (test) + `5bd0a1d` (feat)

**TDD 流程：** RED → GREEN

_Note: 测试文件和实现文件分开提交，遵循 TDD 最佳实践_

## Files Created/Modified

- `src/token_emitter.h` - Token 发射器接口定义
- `src/token_emitter.cpp` - Token 发射器实现，包含三连发和去重逻辑
- `tests/test_token_emitter.cpp` - Token 发射器单元测试，验证三连发和去重功能

## Decisions Made

### 决策 1: 三连发使用相同序列号

**问题**: 如何确保接收端能够正确识别三连发是同一个 Token 帧？

**方案选择**:
- 方案 A: 三次发送使用不同序列号 - 接收端难以识别
- 方案 B: 三次发送使用相同序列号 - **选择此方案**

**理由**:
- 接收端通过序列号自动去重（per D-17, D-18）
- 三连发旨在提高可靠性，而非发送三个不同的 Token
- 序列号在循环外生成，三次发送使用同一个 seq_num

### 决策 2: 三连发立即连续，无延迟间隔

**问题**: 三次发送之间是否需要插入延迟？

**方案选择**:
- 方案 A: 插入延迟增加间隔 - 增加传输时间
- 方案 B: 立即连续发送 - **选择此方案**

**理由**:
- per D-13: 三连发立即连续，无延迟间隔
- 目标是快速击穿，延迟会降低击穿效果
- 使用 bypass_fec=true 跳过 FEC 编码，进一步减少延迟

### 决策 3: 使用 bypass_fec=true 跳过 FEC 编码

**问题**: 控制帧是否需要 FEC 编码？

**方案选择**:
- 方案 A: 使用 FEC 编码增加可靠性 - 增加延迟
- 方案 B: 跳过 FEC 编码 - **选择此方案**

**理由**:
- per P1-6: 控制帧 FEC 编码导致延迟积压
- 三连发已经提供足够的可靠性
- 控制帧需要快速传输，避免延迟积压

## Deviations from Plan

无偏差 - 计划执行完全符合预期。

## Issues Encountered

无问题 - 所有任务按计划完成。

## 实现的需求

- **CTRL-06**: Token 帧三连发击穿机制
  - send_token_triple() 连续调用 3 次 send_packet
  - 三次发送使用相同的序列号
  - 三次发送立即连续，无延迟间隔
  - bypass_fec=true 跳过 FEC 编码
  - is_duplicate_token() 接收端序列号去重

## 后续影响

### Token 发射器使用

服务端调度器可使用：
```cpp
send_token_triple(transmitter, target_node, duration_ms);
```

接收端去重：
```cpp
uint32_t last_seq = 0;
if (!is_duplicate_token(token.seq_num, last_seq)) {
    // 处理新的 Token 帧
}
```

### 性能影响

- 三连发击穿提高控制帧可靠性（per D-11）
- bypass_fec=true 减少控制帧延迟（per P1-6）
- 接收端去重确保只处理新的 Token 帧

### 依赖关系

- 依赖 01-01 (Radiotap 模板): 控制帧使用 MCS 0 + 20MHz
- 依赖 01-02 (序列号生成器): 提供全局单调递增序列号
- 依赖 01-03 (bypass_fec 参数): 支持跳过 FEC 编码

## Self-Check

- [x] 创建的文件存在：src/token_emitter.h, src/token_emitter.cpp, tests/test_token_emitter.cpp
- [x] 提交存在：a4caf1a (RED), 5bd0a1d (GREEN)
- [x] 需求 CTRL-06 已实现
- [x] TDD 流程完整：RED → GREEN

**Self-Check: PASSED**

---

**执行日期**: 2026-04-22
**执行时长**: 10 分钟
**执行模式**: TDD (RED → GREEN)
**状态**: 完成
