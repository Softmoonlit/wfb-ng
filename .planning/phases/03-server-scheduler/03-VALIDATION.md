---
phase: 03
slug: server-scheduler
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-22
---

# Phase 03 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | assert (C++ simple tests) |
| **Config file** | none — existing Makefile |
| **Quick run command** | `./tests/test_scheduler && ./tests/test_aq_sq` |
| **Full suite command** | `make test` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `./tests/test_scheduler`
- **After every plan wave:** Run `make test`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 03-01-01 | 01 | 1 | SCHED-11 | — | N/A | unit | `./tests/test_aq_sq` | ❌ W0 | ⬜ pending |
| 03-01-02 | 01 | 1 | SCHED-10 | — | N/A | unit | `./tests/test_aq_sq` | ❌ W0 | ⬜ pending |
| 03-02-01 | 02 | 1 | SCHED-01 | — | N/A | unit | `./tests/test_scheduler` | ✅ 扩展 | ⬜ pending |
| 03-02-02 | 02 | 1 | SCHED-02 | — | N/A | unit | `./tests/test_scheduler` | ✅ 扩展 | ⬜ pending |
| 03-02-03 | 02 | 1 | SCHED-03 | — | N/A | unit | `./tests/test_scheduler` | ✅ 已有 | ⬜ pending |
| 03-02-04 | 02 | 1 | SCHED-04 | — | N/A | unit | `./tests/test_scheduler` | ✅ 已有 | ⬜ pending |
| 03-02-05 | 02 | 1 | SCHED-05~08 | — | N/A | unit | `./tests/test_scheduler` | ✅ 已有 | ⬜ pending |
| 03-03-01 | 03 | 2 | SCHED-12 | — | N/A | unit | `./tests/test_breathing` | ❌ W0 | ⬜ pending |
| 03-04-01 | 04 | 2 | SCHED-09 | — | N/A | unit | `./tests/test_aq_sq` | ❌ W0 | ⬜ pending |
| 03-04-02 | 04 | 2 | SCHED-13 | — | N/A | unit | `./tests/test_guard_interval` | ✅ 已有 | ⬜ pending |
| 03-04-03 | 04 | 2 | SCHED-14 | — | N/A | unit | `./tests/test_threads` | ✅ 已有 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/test_aq_sq.cpp` — stubs for SCHED-09, SCHED-10, SCHED-11
- [ ] `tests/test_breathing.cpp` — stubs for SCHED-12
- [ ] `src/aq_sq_manager.h` — AQ/SQ 管理器接口
- [ ] `src/aq_sq_manager.cpp` — 队列管理实现
- [ ] `src/breathing_cycle.h` — 呼吸周期接口
- [ ] `src/breathing_cycle.cpp` — 呼吸周期实现

*Existing infrastructure covers partial phase requirements.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| 多节点集群调度 | SCHED-01~14 | 需要硬件环境 | 10节点实测，观察碰撞率 |
| 频谱占用验证 | SCHED-09 | 需要频谱仪 | 全员深睡时频谱占用 < 15% |

*Hardware-dependent tests are deferred to integration testing.*

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
