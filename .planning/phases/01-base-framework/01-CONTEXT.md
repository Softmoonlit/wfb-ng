# Phase 1: 基础框架与数据结构 - Context

**Gathered:** 2026-04-22
**Status:** Ready for planning

<domain>
## Phase Boundary

建立 Token Passing 的核心数据结构和基础工具，包括控制帧结构、序列号生成、Radiotap 模板、FEC 跳过、三连发、保护间隔和双模板分流。此阶段不涉及进程合并、线程架构或调度器状态机（这些在 Phase 2 和 Phase 3）。

</domain>

<decisions>
## Implementation Decisions

### 序列号生成机制（CTRL-03）
- **D-01:** 使用 `std::atomic<uint32_t>` 实现全局序列号生成器，确保线程安全
- **D-02:** 序列号从 0 开始初始化，服务器重启后重置
- **D-03:** 生成器封装为全局变量 `std::atomic<uint32_t> g_token_seq_num{0}`，通过 `g_token_seq_num.fetch_add(1)` 递增

### Radiotap 模板设计（CTRL-04, CTRL-08）
- **D-04:** 最低 MCS 模板固定使用 MCS 0 + 20MHz 频宽（理论速率 6Mbps），确保控制帧全网覆盖
- **D-05:** 高阶 MCS 模板动态生成，基于 wfb_tx CLI 参数（`-M` mcs_index、`-B` bandwidth），在进程启动时初始化
- **D-06:** 复用现有 `init_radiotap_header()` 函数创建模板，创建两个全局静态变量：
  - `radiotap_header_t g_control_radiotap`（固定：MCS 0, 20MHz）
  - `radiotap_header_t g_data_radiotap`（动态：基于 CLI 参数）
- **D-07:** 发射时根据帧类型通过指针切换模板，零延迟

### 控制帧跳过 FEC（CTRL-05）
- **D-08:** 在发射函数中添加 `bool bypass_fec` 参数，根据标志跳过 FEC 编码逻辑
- **D-09:** 复用现有的加密和 Radiotap 封装逻辑，仅跳过 FEC 编码步骤
- **D-10:** 控制帧发射调用 `send_packet(buf, size, flags, /*bypass_fec=*/true)` 或新增专用函数

### 三连发击穿机制（CTRL-06）
- **D-11:** 在 `send_token()` 函数内部循环调用 3 次 `inject_packet()`，封装三连发细节
- **D-12:** 三次发送使用相同的序列号，接收端通过序列号自动去重
- **D-13:** 三次发送立即连续，无延迟间隔，最大化击穿概率

### 保护间隔插入（CTRL-07）
- **D-14:** 5ms 保护间隔由调度器在节点切换前插入
- **D-15:** 使用 `timerfd` + 截止前极短时间（<100μs）空转自旋实现高精度休眠
- **D-16:** 保护间隔函数封装为 `void apply_guard_interval(uint16_t duration_ms)`，供调度器调用

### 接收端去重逻辑（CTRL-06 相关）
- **D-17:** 接收端使用单序列号比较，记录 `last_seq`，新包的 `seq_num <= last_seq` 时丢弃
- **D-18:** 去重逻辑封装为 `bool is_duplicate_token(uint32_t seq_num, uint32_t& last_seq)` 函数

### Radiotap 字段规范（CTRL-04 相关）
- **D-19:** 控制帧的 Radiotap 头部使用标准字段（Rate、Channel、MCS 等），无需额外 QoS 或 TX_FLAGS
- **D-20:** 保持与现有 `radiotap_header_t` 结构一致，避免特殊扩展字段

### Claude's Discretion
- Radiotap 头部的具体字节布局细节
- 序列号溢出后的处理策略（uint32_t 范围足够大，极少发生）
- 控制帧发射失败时的重试逻辑

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### 项目文档
- `.planning/PROJECT.md` — 项目核心价值和三层架构原则
- `.planning/REQUIREMENTS.md` — Phase 1 需求（CTRL-01 ~ CTRL-08）
- `.planning/ROADMAP.md` — Phase 1 成功标准
- `.planning/research/STACK.md` — MCS 速率映射表（MCS 0 = 6Mbps, 20MHz）

### 现有代码文件
- `src/mac_token.h` — TokenFrame 结构定义（已实现）
- `src/mac_token.cpp` — 序列化/反序列化函数（已实现）
- `src/tx.hpp` — Transmitter 类、radiotap_header_t 结构、init_radiotap_header() 函数
- `src/tx.cpp` — Transmitter::send_packet() FEC 编码逻辑
- `tests/test_mac_token.cpp` — 现有单元测试示例
- `wfb_ng/services.py` — wfb_tx CLI 参数传递逻辑（-M, -B）

### 外部规范
- `src/ieee80211_radiotap.h` — Radiotap 头部格式定义
- `src/radiotap.c` — Radiotap 解析器实现

</canonical_refs>

<code_context>
## Existing Code Insights

### 已实现模块
- **TokenFrame 结构**：`src/mac_token.h` 已定义 `struct TokenFrame`（magic, target_node, duration_ms, seq_num），字节对齐无填充
- **序列化函数**：`src/mac_token.cpp` 已实现 `serialize_token()` 和 `parse_token()`
- **单元测试**：`tests/test_mac_token.cpp` 提供测试框架示例

### 可复用资产
- **radiotap_header_t 结构**：`src/tx.hpp` 已定义包含 MCS、带宽、STBC、LDPC、short_gi 等参数
- **init_radiotap_header() 函数**：`src/tx.hpp` 支持动态初始化 Radiotap 头部，可直接复用创建控制帧和数据帧模板
- **RawSocketTransmitter::inject_packet()**：底层发射函数，可直接调用或通过 bypass_fec 标志控制
- **Transmitter::send_packet()**：现有数据帧发送路径，包含 FEC 编码逻辑，可作为控制帧发送的参考

### 建立的模式
- **头文件命名**：使用 `src/<module>.h` / `src/<module>.cpp` 命名约定
- **单元测试命名**：使用 `tests/test_<module>.cpp` 命名约定
- **全局静态变量**：项目中已有全局配置变量模式（如 `settings`），`g_control_radiotap` 和 `g_data_radiotap` 符合此模式

### 集成点
- **wfb_tx 启动流程**：`wfb_ng/services.py` 中 `init_udp_direct_tx()` 函数传递 CLI 参数，是高阶 MCS 模板动态初始化的入口点
- **发射路径**：控制帧和数据帧最终都通过 `RawSocketTransmitter::inject_packet()` 发射，需要在此处根据帧类型切换 Radiotap 模板

</code_context>

<specifics>
## Specific Ideas

- **三连发立即连续**：三次 Token 发送无延迟间隔，基于 Token 帧极小（8 字节）的特性，立即连续发送能最快覆盖接收端
- **序列号从 0 开始**：系统重启后所有旧包已失效，无需复杂的时间戳或随机数初始化
- **全局静态双模板**：`g_control_radiotap` 和 `g_data_radiotap` 作为全局静态变量，发射时指针切换，零延迟且内存开销小
- **单序列号去重**：接收端仅记录 `last_seq`，`seq_num <= last_seq` 时丢弃，uint32_t 范围足够大（42 亿），回绕极少见
- **保护间隔精度**：使用 `timerfd` + 空转自旋确保微秒级精度，避免 `usleep()` 的毫秒级抖动

</specifics>

<deferred>
## Deferred Ideas

- **MAC 地址分配**：控制帧的源 MAC 地址和目标 MAC 地址分配策略，留待 Phase 2 网络层设计
- **加密集成**：控制帧是否需要加密，以及加密密钥管理，留待安全设计阶段
- **多网卡支持**：双模板在多网卡环境下的发射策略，留待 Phase 2 多线程架构
- **统计信息**：Token 发送成功率、去重统计等监控指标，留待 Phase 3 可观测性设计
- **性能优化**：Radiotap 头部预计算缓存、批量发送优化，留待 Phase 4 性能调优

</deferred>

---

*Phase: 01-base-framework*
*Context gathered: 2026-04-22*
