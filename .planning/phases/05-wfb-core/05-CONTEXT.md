# Phase 5: 单进程 wfb_core 架构实现 - Context

**Gathered:** 2026-04-28
**Status:** Ready for planning

<domain>
## Phase Boundary

实现真正的单进程 wfb_core 架构，接管原有的 wfb_tx / wfb_rx / wfb_tun 多进程方案。控制面与发射端在单进程中闭环，实现完全的射频零碰撞与高效的反压调度，为底层传输提供坚实基础。

</domain>

<decisions>
## Implementation Decisions

### 进程架构入口
- **D-01:** `wfb_core` 将作为唯一正式入口，旧的 `wfb_tx` / `wfb_rx` / `wfb_tun` 只保留用于开发调试和向后兼容，不再作为生产环境的推荐方式。
- **D-02:** `server_start.sh` 和 `client_start.sh` 将改写为薄包装，负责系统准备（如网卡 Monitor 模式、创建 TUN）后，直接启动 `wfb_core` 进程。

### Claude's Discretion
- `wfb_core` 的命令行参数风格（如采用 `--mode server/client` 或其他模式），以最大限度保持与现有系统运维习惯的一致性为主，由后续规划自行决定。
- 内部线程的具体拆分方式（如确保控制面优先级 99、发射端 90、TUN 读取不提权等）在遵循前置阶段确立的 `threads.h` 框架下由规划器细化。
- 日志系统与退出信号（SIGINT/SIGTERM）的统一处理策略。

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 进程架构约束
- `.planning/PROJECT.md` — 合并进程架构（单进程内实现 TUN 读取和空口发射），禁止独立进程通过 UDP 通信环路。
- `.planning/REQUIREMENTS.md` — PROC-01 ~ PROC-12（单进程与调度骨架需求），RT-01 ~ RT-08（实时调度提权及 5ms 保护间隔），BUF-01 ~ BUF-10（动态水位线与 txqueuelen 100）。
- `.planning/research/ARCHITECTURE.md` — 操作系统层极小水池反压及 MAC 层接管逻辑的参考设计。

### 前置阶段已锁定逻辑
- `.planning/phases/02-process-architecture/02-CONTEXT.md` (或同等汇总摘要) — `src/threads.h` 已经锁定了线程优先级分配机制、共享状态结构 `ThreadSharedState` 及相关的 `can_send`、`token_expire_time_ms` 变量。
- `.planning/phases/01-base-framework/01-CONTEXT.md` — `bypass_fec` 与 TokenFrame 三连发、保护间隔（`kGuardIntervalMs`=5）均在单进程融合中必须得到延续。

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `src/wfb_core.cpp` 现已包含了基本的线程编排主循环（`control_main_loop`、`tx_main_loop`、`tun_reader_main_loop`），需要进一步完善真实数据流路径。
- `src/threads.h` 和相关测试已经跑通了优先级提权和门控条件变量逻辑，可作为主要基础设施复用。
- `scripts/server_start.sh` 及 `scripts/client_start.sh` 已有完备的 TUN、Monitor 模式初始化环境检测函数，仅需将启动子进程段更改为调用 `wfb_core`。

### Established Patterns
- 使用 `std::condition_variable` 来唤醒发射线程（无 `usleep` 轮询）。
- 通过 `get_monotonic_ms()` 判断 Token 过期时间。

### Integration Points
- `wfb_core` 将直接整合 `tx.cpp`、`rx.cpp` 与 `wfb_tun.c` 的处理逻辑，直接在内部分发 IP 数据包并执行 Radiotap 模板封装和底层 Inject，免去基于 localhost 的 UDP IPC 步骤。

</code_context>

<specifics>
## Specific Ideas

- 单进程的日志应当比原有多进程输出更内聚，最好提供结构化或标记清晰的单文件日志输出。

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope.

</deferred>

---

*Phase: 05-wfb-core*
*Context gathered: 2026-04-28*
