# 代码库结构

**Analysis Date:** 2026-04-22

## Directory Layout

```
D:\Code\wfb-ng/
├── src/            # 核心源代码目录
├── tests/          # 单元测试与集成测试
└── docs/           # (假设存在) 文档与设计资料
```

## Directory Purposes

**src:**
- Purpose: 包含所有的生产级别代码，包括物理层帧处理、OS层调度及队列、以及应用层的 FEC 实现。
- Contains: C++ 源文件 (.cpp, .h, .hpp)。
- Key files: `server_scheduler.cpp`, `watermark.cpp`, `mac_token.cpp`, `packet_queue.h`

**tests:**
- Purpose: 存放用于验证各个独立模块功能的自动化测试代码。
- Contains: C++ 测试用例源文件。
- Key files: `test_scheduler.cpp`, `test_watermark.cpp`, `test_mac_token.cpp`, `test_packet_queue.cpp`

## Key File Locations

**Entry Points:**
- `src/tx.cpp`: 物理层发送端主入口（注入模式）。
- `src/rx.cpp`: 物理层接收端主入口（混杂模式捕获）。
- `src/wifibroadcast.cpp`: 综合总控。

**Configuration:**
- `src/tx_cmd.h` / `src/ieee80211_radiotap.h`: 包含配置结构和特定网卡底层的配置信息。

**Core Logic:**
- `src/server_scheduler.cpp`: 核心服务端三态扩窗调度逻辑。
- `src/watermark.cpp`: 动态水位计算逻辑。
- `src/mac_token.cpp`: 控制帧序列化解析。

**Testing:**
- `tests/test_scheduler.cpp`: 针对三态调度的边界测试。
- `tests/test_packet_queue.cpp`: 针对队列多线程并发的安全性测试。

## Naming Conventions

**Files:**
- snake_case: 例如 `server_scheduler.cpp`, `packet_queue.h`

**Directories:**
- 全部小写且使用短名称: 例如 `src`, `tests`

## Where to Add New Code

**New Feature (调度策略调整或协议修改):**
- Primary code: `src/server_scheduler.cpp` / `src/mac_token.cpp`
- Tests: `tests/test_scheduler.cpp` / `tests/test_mac_token.cpp`

**New Component/Module (如新的 FEC 算法):**
- Implementation: `src/` 目录下新增对应前缀的 `.cpp` 和 `.h`。

**Utilities:**
- Shared helpers: 放在 `src/` 下，若极度通用可建类似 `src/utils/`（当前为扁平化结构，直接放置于 `src/`）。

## Special Directories

**src:**
- Purpose: 代码核心
- Generated: No
- Committed: Yes

**tests:**
- Purpose: 单元测试
- Generated: No
- Committed: Yes

---

*Structure analysis: 2026-04-22*