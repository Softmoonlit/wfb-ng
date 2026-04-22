# Phase 4: 集成与优化 - Context

**Gathered:** 2026-04-22
**Status:** Ready for planning

<domain>
## Phase Boundary

控制帧 Radiotap 模板分流、三连发击穿、MCS 映射表集成、UFTP 配置指南，完成端到端集成和优化。这是最后一个阶段，将所有已完成的模块（基础框架、进程架构、调度引擎）集成到一起，并提供应用层（UFTP）的配置指南和集成验证。此阶段不涉及新功能开发，而是验证、文档化和优化现有实现。

</domain>

<decisions>
## Implementation Decisions

### UFTP 配置文档格式与内容（APP-01, APP-02）

- **D-01:** 使用纯 Markdown 格式编写，放在项目内 `docs/` 目录
- **D-02:** 文档内容包含完整配置示例和原理解释
- **D-03:** 限速计算公式通过多场景示例表格展示，覆盖不同 MCS 和节点数组合
- **D-04:** 提供真实配置示例、完整命令行参数说明、底层原理阐述
- **D-05:** 限速公式：`rate_limit = (R_phy / N) × 1.2`，其中 R_phy 为物理速率，N 为节点数
- **D-06:** 超时调整建议：适度调大 GRTT 和重传次数，容忍秒级合理波动

### 端到端集成测试策略

- **D-07:** 测试场景覆盖单节点基础测试和多节点并发测试（5-10 节点）
- **D-08:** 测试数据使用 40MB 模型文件，真实场景数据规模
- **D-09:** 全自动化测试脚本，一键运行所有测试并生成报告
- **D-10:** 性能指标验证：碰撞率（目标 0%）、带宽利用率（目标 > 90%）、延迟指标（控制帧 < 10ms，数据包 < 2s）
- **D-11:** 测试结果报告采用纯文本格式，包含表格和说明

### 启动脚本设计

- **D-12:** 使用 Bash Shell 脚本编写
- **D-13:** 配置方式：配置文件 + 命令行参数覆盖，灵活可维护
- **D-14:** 系统检查项：Root 权限检查、网卡状态检查（是否 Monitor 模式）
- **D-15:** 日志输出：日志文件 + 标准输出，便于调试和生产环境日志收集
- **D-16:** 错误处理策略：分类错误处理，根据错误类型选择不同处理方式

### 应用层集成验证方法（APP-03, APP-04）

- **D-17:** 验证方法：网络抓包分析（tcpdump/wireshark）+ 应用层日志分析
- **D-18:** 路由配置：特定 IP 段路由，仅目标 IP 匹配时经过 TUN 设备，不影响其他应用
- **D-19:** 验证内容：
  - TUN 设备拦截验证（UFTP 数据包经过 TUN 且被正确处理）
  - 控制帧机制验证（Radiotap 模板切换、三连发、保护间隔）
  - 零感知验证（UFTP 无需修改源码，无需感知底层调度）
- **D-20:** 验证通过标准：严格标准，所有验证项必须通过才算集成成功

### Claude's Discretion

- 配置文件的具体格式（INI/YAML/JSON）
- 日志文件的具体路径和命名规则
- 测试脚本的错误分类细节
- 抓包分析的具体命令和过滤规则

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目文档
- `.planning/PROJECT.md` — 项目核心价值和三层架构原则
- `.planning/REQUIREMENTS.md` — Phase 4 需求（APP-01 ~ APP-04）
- `.planning/ROADMAP.md` — Phase 4 成功标准
- `.planning/research/STACK.md` — MCS 速率映射表（限速计算依据）
- `.planning/research/ARCHITECTURE.md` — 三层架构（应用层、操作系统层、MAC/物理层）

### 已实现代码
- `src/wfb_core.cpp` — 单进程主循环（已完成）
- `src/server_scheduler.h/cpp` — 服务端调度器（已完成）
- `src/radiotap_template.h/cpp` — Radiotap 双模板（已完成）
- `src/token_emitter.h/cpp` — Token 三连发（已完成）
- `src/mac_token.h/cpp` — TokenFrame 结构（已完成）
- `src/threads.h` — 线程架构定义（已完成）

### 测试参考
- `tests/test_scheduler.cpp` — 调度器单元测试
- `tests/test_aq_sq.cpp` — AQ/SQ 管理测试
- `tests/test_breathing.cpp` — 呼吸周期测试

</canonical_refs>

<code_context>
## Existing Code Insights

### 已完成模块
- **基础框架（Phase 1）**：TokenFrame、Radiotap 双模板、序列号生成器、三连发、保护间隔
- **进程架构（Phase 2）**：单进程架构、实时调度、环形队列、动态水位线
- **调度引擎（Phase 3）**：三态状态机、呼吸周期、AQ/SQ 管理、空闲巡逻、混合交织

### 可复用资产
- **配置文件模式**：现有 wfb-ng 使用命令行参数传递配置，可扩展为配置文件
- **日志系统**：现有日志输出到 stdout，可扩展为文件日志
- **测试框架**：现有 Google Test 单元测试框架，可直接复用
- **脚本模式**：现有 `scripts/tx_standalone.sh` 和 `scripts/rx_standalone.sh` 可作为参考

### 集成点
- **应用层入口**：UFTP 通过标准网络接口（eth0）发送 UDP 包，路由配置将特定 IP 段导向 TUN 设备
- **TUN 读取**：`wfb_core.cpp` 中的 `tun_reader_thread` 读取 TUN 设备，获取 UFTP 发出的 IP 包
- **控制帧发射**：`token_emitter.cpp` 中的三连发机制和 Radiotap 模板切换
- **调度器集成**：`server_scheduler.cpp` 控制发射时机，应用层无感知

### 关键参数传递
- **MCS 映射表**：已实现 MCS 速率映射，可从配置文件或命令行参数读取
- **节点配置**：节点数、IP 地址、网卡接口等配置需文档化
- **限速计算**：基于物理速率和节点数动态计算

</code_context>

<specifics>
## Specific Ideas

- **UFTP 配置文档命名**：`docs/uftp_config.md`，包含限速公式、超时调整、配置示例
- **限速计算示例表格**：
  ```
  | MCS | 频宽 (MHz) | 物理速率 (Mbps) | 节点数 | 限速值 (Mbps) |
  |-----|----------|----------------|--------|--------------|
  | 0   | 20       | 6              | 5      | 1.44         |
  | 0   | 20       | 6              | 10     | 0.72         |
  | 6   | 20       | 54             | 5      | 12.96        |
  | 6   | 20       | 54             | 10     | 6.48         |
  ```
- **集成测试脚本**：`tests/integration_test.sh`，自动启动服务器和客户端，运行 UFTP 传输 40MB 文件，验证性能指标
- **启动脚本配置文件**：`config/server.conf` 和 `config/client.conf`，使用 INI 或 YAML 格式
- **路由配置示例**：`ip route add 192.168.100.0/24 dev tun0`，将客户端 IP 段路由到 TUN 设备

</specifics>

<deferred>
## Deferred Ideas

- **性能优化**：FEC 编码使用 SSE/AVX 指令优化 — v2 版本
- **多网卡支持**：双网卡负载均衡和故障切换 — v2 版本
- **容器化部署**：Docker/K8s 部署方案 — v2 版本
- **监控 UI**：实时 Web UI 显示调度状态 — v2 版本
- **Prometheus 集成**：指标导出和监控系统集成 — v2 版本

### Reviewed Todos (not folded)

无 — Phase 4 已覆盖所有集成和优化需求

</deferred>

---

*Phase: 04-integration-optimization*
*Context gathered: 2026-04-22*
