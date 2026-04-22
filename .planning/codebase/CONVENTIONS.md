# 代码规范与风格约定

**分析日期:** 2026-04-22

## 命名约定

**文件命名:**
- C++ 源码与头文件均采用全小写与下划线 (snake_case)，例如: `mac_token.cpp`, `test_mac_token.cpp`。

**结构体与类:**
- 采用大驼峰命名 (PascalCase)，例如: `TokenFrame`。

**变量与函数:**
- 采用全小写与下划线 (snake_case)，例如: `serialize_token`, `target_node`。

## 代码结构与内存管理

**头文件保护:**
- 统一使用 `#pragma once`，避免重复包含。

**结构体对齐:**
- 涉及网络传输或序列化的底层控制帧（如 `src/mac_token.h`），使用 `#pragma pack(push, 1)` 和 `#pragma pack(pop)` 确保紧凑对齐。

**类型定义:**
- 采用定宽整数类型（如 `uint8_t`, `uint16_t`, `uint32_t`）以保证跨平台一致性。

## 注释与文档规范

**注释语言:**
- 按照项目指令要求，所有的代码注释必须完全使用中文。例如在 `src/mac_token.h` 中有 `// 控制帧结构体，脱离 FEC 与 IP 路由体系` 的中文注释。

**Markdown 文档:**
- 所有的 Markdown 文档内容、文件名必须使用全中文，且在非时间敏感情况下不带日期前缀。

---

*规范分析日期: 2026-04-22*
