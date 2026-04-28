# Phase 05: wfb-core - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-28
**Phase:** 05-wfb-core
**Areas discussed:** 入口归一（推荐）

---

## 入口归一（推荐）

| Option | Description | Selected |
|--------|-------------|----------|
| wfb_core 作为唯一入口 | Phase 5 将完全采用单进程的 wfb_core。旧的 wfb_tx / rx / tun 只用于开发调试或向后兼容，不再是推荐和主推的方式。 | ✓ |
| 保留多进程和单进程双入口 | 除了 wfb_core 外，旧的多进程套件仍作为长期支持选项，两者并存，维护双路径文档和脚本。 | |

**User's choice:** wfb_core 作为唯一入口
**Notes:** 仅讨论了入口选项，其余如参数、脚本替换等交给 Claude 自行决策（Claude's Discretion）。

---

## Claude's Discretion

- 模式定义（参数化结构等）
- 脚本策略（薄包装设计）
- 线程边界（合并/切分）

## Deferred Ideas

None
