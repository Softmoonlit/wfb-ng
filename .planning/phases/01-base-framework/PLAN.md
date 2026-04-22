# Phase 1: 基础框架与数据结构 - 主计划

**阶段**: Phase 1 - 基础框架与数据结构
**创建日期**: 2026-04-22
**状态**: ✅ 验证通过，待执行

---

## 阶段目标

建立 Token Passing 的核心数据结构和基础工具，为后续架构搭建提供坚实基础。

**核心价值**:
- 定义超轻量级 MAC 控制帧结构（TokenFrame）
- 实现全局单调递增序列号生成器
- 实现 Radiotap 双模板分流系统
- 实现控制帧三连发击穿机制
- 实现高精度保护间隔函数

---

## 需求映射

| 需求 ID | 描述 | 计划 | 状态 |
|---------|------|------|------|
| CTRL-01 | 定义超轻量级 MAC 控制帧结构（TokenFrame） | 01-00 | ✅ 已实现，待验证 |
| CTRL-02 | 实现 TokenFrame 序列化和反序列化函数 | 01-00 | ✅ 已实现，待验证 |
| CTRL-03 | 实现全局单调递增序列号生成 | 01-02 | ○ 待实现 |
| CTRL-04 | 控制帧使用 Radiotap 最低 MCS 速率模板封装 | 01-01 | ○ 待实现 |
| CTRL-05 | 控制帧跳过 FEC 前向纠错编码 | 01-03 | ○ 待实现 |
| CTRL-06 | 发送 Token 时使用三连发击穿机制 | 01-04 | ○ 待实现 |
| CTRL-07 | 服务器在节点切换时插入 5ms 保护间隔 | 01-05 | ○ 待实现 |
| CTRL-08 | 控制帧发射使用双 Radiotap 模板分流 | 01-01 | ○ 待实现 |

---

## 成功标准

1. ✅ TokenFrame 结构体正确定义，字节对齐无填充
2. ✅ 序列化/反序列化函数通过单元测试，验证字节正确性
3. ○ 序列号生成器线程安全，全局单调递增
4. ○ Radiotap 模板正确封装最低 MCS 速率
5. ○ 控制帧发射路径跳过 FEC 编码逻辑
6. ○ 三连发机制实现，接收端序列号去重正常
7. ○ 保护间隔插入逻辑正确，节点切换无碰撞
8. ○ 双模板分流实现，发射时零延迟切换

---

## 执行计划

### Wave 结构

```
Wave 0 (验证现有实现):
  └─ 01-00-PLAN.md — 验证 TokenFrame 实现

Wave 1 (并行执行):
  ├─ 01-01-PLAN.md — 实现 Radiotap 双模板分流系统
  ├─ 01-02-PLAN.md — 实现全局单调递增序列号生成器
  └─ 01-05-PLAN.md — 实现高精度保护间隔函数

Wave 2 (依赖 Wave 0):
  └─ 01-03-PLAN.md — 修改 send_packet 添加 bypass_fec 参数

Wave 3 (依赖 Wave 1 和 Wave 2):
  └─ 01-04-PLAN.md — 实现 Token 三连发发射器
```

### 计划详情

#### Wave 0: 验证现有实现

**01-00-PLAN.md** — 验证 TokenFrame 实现
- 需求: CTRL-01, CTRL-02
- 文件: `tests/test_mac_token.cpp`（补充测试）
- 任务: 3 个（验证字节对齐、序列化、反序列化）
- 依赖: 无
- 状态: 待执行

---

#### Wave 1: 并行执行（无依赖）

**01-01-PLAN.md** — 实现 Radiotap 双模板分流系统
- 需求: CTRL-04, CTRL-08
- 文件: `src/radiotap_template.{h,cpp}`, `tests/test_radiotap_template.cpp`
- 任务: 3 个（定义双模板、实现初始化、实现切换）
- 依赖: 无
- 状态: 待执行

**01-02-PLAN.md** — 实现全局单调递增序列号生成器
- 需求: CTRL-03
- 文件: `src/token_seq_generator.{h,cpp}`, `tests/test_token_seq_generator.cpp`
- 任务: 3 个（定义接口、实现原子递增、编写测试）
- 依赖: 无
- 状态: 待执行

**01-05-PLAN.md** — 实现高精度保护间隔函数
- 需求: CTRL-07
- 文件: `src/guard_interval.{h,cpp}`, `tests/test_guard_interval.cpp`
- 任务: 3 个（定义接口、实现 timerfd + 空转自旋、编写测试）
- 依赖: 无
- 状态: 待执行

---

#### Wave 2: 依赖 Wave 0

**01-03-PLAN.md** — 修改 send_packet 添加 bypass_fec 参数
- 需求: CTRL-05
- 文件: `src/tx.{hpp,cpp}`, `tests/test_bypass_fec.cpp`
- 任务: 3 个（添加参数、修改 FEC 逻辑、编写测试）
- 依赖: 01-00（需要 TokenFrame 定义）
- 状态: 待执行

---

#### Wave 3: 依赖 Wave 1 和 Wave 2

**01-04-PLAN.md** — 实现 Token 三连发发射器
- 需求: CTRL-06
- 文件: `src/token_emitter.{h,cpp}`, `tests/test_token_emitter.cpp`
- 任务: 3 个（实现三连发发射、实现接收端去重、编写测试）
- 依赖: 01-01（Radiotap 模板）, 01-02（序列号生成器）, 01-03（bypass_fec）
- 状态: 待执行

---

## 关键决策

以下决策已在 CONTEXT.md 中确定，计划必须遵守：

### 序列号生成机制（CTRL-03）
- **D-01**: 使用 `std::atomic<uint32_t>` 确保线程安全
- **D-02**: 序列号从 0 开始初始化
- **D-03**: 封装为全局变量 `g_token_seq_num`

### Radiotap 模板设计（CTRL-04, CTRL-08）
- **D-04**: 最低 MCS 模板固定使用 MCS 0 + 20MHz
- **D-05**: 高阶 MCS 模板动态生成，基于 CLI 参数
- **D-06**: 复用 `init_radiotap_header()` 函数创建模板
- **D-07**: 发射时根据帧类型通过指针切换模板

### 控制帧跳过 FEC（CTRL-05）
- **D-08**: 在发射函数中添加 `bool bypass_fec` 参数
- **D-09**: 复用现有加密和 Radiotap 封装逻辑
- **D-10**: 控制帧发射调用 `send_packet(buf, size, flags, true)`

### 三连发击穿机制（CTRL-06）
- **D-11**: 在 `send_token()` 内部循环调用 3 次 `inject_packet()`
- **D-12**: 三次发送使用相同的序列号
- **D-13**: 三次发送立即连续，无延迟间隔

### 保护间隔插入（CTRL-07）
- **D-14**: 5ms 保护间隔由调度器在节点切换前插入
- **D-15**: 使用 `timerfd` + 空转自旋实现高精度休眠
- **D-16**: 封装为 `apply_guard_interval()` 函数

### 接收端去重逻辑（CTRL-06 相关）
- **D-17**: 接收端使用单序列号比较，记录 `last_seq`
- **D-18**: 去重逻辑封装为 `is_duplicate_token()` 函数

---

## 测试策略

### 单元测试
- 每个计划包含独立的单元测试
- 测试命名：`tests/test_<module>.cpp`
- 测试框架：Catch2 或 Google Test
- 覆盖率要求：> 80%

### 集成测试
- Phase 1 结束时执行端到端集成测试
- 验证完整流程：序列号生成 → Token 发射 → 接收端去重

### 性能测试
- 保护间隔精度测试（目标：< 100μs 误差）
- 序列号生成器并发测试（目标：无线程竞争）
- Radiotap 模板切换延迟测试（目标：零延迟）

---

## 验收标准

### 代码质量
- ✅ 遵循 Google C++ Style Guide
- ✅ 所有函数和关键逻辑添加中文注释
- ✅ 通过 Cppcheck 和 clang-tidy 静态分析
- ✅ 使用 RAII 和智能指针，避免裸指针

### 测试覆盖
- ✅ 所有单元测试通过
- ✅ 测试覆盖率 > 80%
- ✅ 无内存泄漏（Valgrind 验证）

### 功能验证
- ✅ 满足所有 8 个成功标准
- ✅ 满足所有 8 个需求（CTRL-01 ~ CTRL-08）
- ✅ 关键决策（D-01 ~ D-20）全部实现

---

## 风险与缓解

### 技术风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 修改核心发射函数引入 bug | 中 | P1 | 01-03 拆分为独立计划，增加测试覆盖 |
| timerfd 权限不足 | 低 | P1 | 启动时检测 Root 权限，明确警告 |
| Radiotap 模板与硬件不匹配 | 中 | P2 | Phase 4 集成测试验证 |

### 进度风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 测试覆盖不足 | 低 | P2 | TDD 驱动开发，强制单元测试 |
| TokenFrame 现有代码缺陷 | 低 | P1 | Wave 0 优先验证，及早发现问题 |

---

## 执行命令

**开始执行**:
```bash
/gsd-execute-phase 01
```

**执行顺序**:
1. Wave 0: 01-00（验证现有实现）
2. Wave 1: 01-01, 01-02, 01-05（并行执行）
3. Wave 2: 01-03（依赖 Wave 0）
4. Wave 3: 01-04（依赖 Wave 1 和 Wave 2）

**预计工作量**: 3-4 天

---

## 文档引用

- **上下文**: `.planning/phases/01-base-framework/01-CONTEXT.md`
- **讨论日志**: `.planning/phases/01-base-framework/01-DISCUSSION-LOG.md`
- **验证报告**: `.planning/phases/01-base-framework/PLAN-CHECK.md`
- **项目文档**: `.planning/PROJECT.md`
- **需求文档**: `.planning/REQUIREMENTS.md`
- **路线图**: `.planning/ROADMAP.md`

---

*主计划创建: 2026-04-22*
*阶段: Phase 1 - 基础框架与数据结构*
*计划数: 6 个子计划*
*状态: ✅ 验证通过，待执行*
