---
phase: 04
plan: 01
subsystem: 应用层集成
tags: [documentation, uftp, configuration, rate-limiting]
requires: []
provides: [APP-01, APP-02]
affects: []
tech-stack:
  added:
    - UFTP 限速配置文档
    - MCS 速率映射表文档化
  patterns:
    - 微超发限速公式
    - GRTT 超时调整策略
key-files:
  created:
    - docs/uftp_config.md
  modified: []
decisions:
  - 限速公式使用 1.2 微超发系数，平衡频谱利用率和反压控制
  - GRTT 建议值 5-10 秒，容忍 Token Passing 调度延迟
metrics:
  duration: 5m
  completed: "2026-04-22T15:53:00Z"
  tasks: 1
  files: 1
---

# Phase 04 Plan 01: UFTP 配置指南文档 Summary

**一一句话总结:** 创建完整的 UFTP 限速配置和超时调整指南文档，确保应用层正确配置以适配底层 Token Passing 调度。

---

## 执行结果

### 任务完成情况

| 任务 | 名称 | 状态 | 提交 | 文件 |
|------|------|------|------|------|
| 1 | 创建 UFTP 限速配置文档 | ✅ 完成 | fa786aa | docs/uftp_config.md |

### 需求映射

| 需求 | 描述 | 状态 |
|------|------|------|
| APP-01 | 提供 UFTP 微超发限速配置指南 | ✅ 完成 |
| APP-02 | 提供 UFTP 超时阈值调整建议 | ✅ 完成 |

---

## 交付物详情

### docs/uftp_config.md

完整的 UFTP 配置指南文档，包含：

1. **限速公式推导**
   - 核心公式：`rate_limit = (R_phy / N) × 1.2`
   - UFTP `-R` 参数计算方法

2. **MCS 速率映射表**
   - MCS 0-8 完整映射
   - 包含频宽、空间流数、调制方式、编码率、理论物理速率

3. **多场景限速计算示例表**
   - 18 个计算示例
   - 覆盖 MCS 0-8 和 5/10 节点组合

4. **超时调整建议**
   - GRTT 建议 5-10 秒
   - 重传次数建议 5-10 次
   - UFTP 断连超时配置建议

5. **完整配置示例**
   - 4 个场景的完整命令行示例
   - 包含计算过程说明

6. **底层原理阐述**
   - 三层架构说明
   - 反压机制
   - 动态水位线
   - Token Passing 调度
   - 呼吸周期

7. **常见问题与调试建议**

---

## 验证结果

| 验收标准 | 结果 |
|----------|------|
| `docs/uftp_config.md` 文件存在 | ✅ 通过 |
| 包含字符串 "rate_limit = (R_phy / N) × 1.2" | ✅ 通过 |
| 包含 MCS 速率映射表 | ✅ 通过 |
| 包含至少 3 个多场景限速计算示例 | ✅ 通过（18 个示例） |
| 包含 "GRTT" 和 "重传次数" 超时调整建议 | ✅ 通过 |
| 包含完整的 UFTP 命令行配置示例 | ✅ 通过（4 个场景） |

---

## 偏差记录

### 自动修复的问题

无 - 计划按预期执行。

---

## 已知遗留项

无。

---

## 后续建议

1. 根据实际部署环境测试，可能需要调整 MCS 映射表中的速率值
2. 建议补充更多场景的配置示例（如不同信道条件下的调整策略）
3. 可考虑添加自动化脚本计算 `-R` 参数值

---

## Self-Check: PASSED

- [x] 文件存在：`docs/uftp_config.md`
- [x] 提交存在：`fa786aa`
- [x] 需求 APP-01 已实现
- [x] 需求 APP-02 已实现

---

*Summary 创建: 2026-04-22*
*执行时长: 约 5 分钟*
