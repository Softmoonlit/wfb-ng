---
phase: 05-wfb-core
verified: 2026-04-28T23:30:00Z
status: gaps_found
score: 20/39 must-haves verified
overrides_applied: 0
gaps:
  - truth: "wfb_core 进程能够成功启动并解析命令行参数"
    status: failed
    reason: "编译失败，ServerScheduler构造函数参数不匹配"
    artifacts:
      - path: "src/wfb_core.cpp"
        issue: "ServerScheduler scheduler(config.mcs, config.bandwidth) 构造函数不存在"
      - path: "src/server_scheduler.h"
        issue: "ServerScheduler类没有接受两个参数的构造函数"
    missing:
      - "添加ServerScheduler构造函数或修改调用方式"
  - truth: "RX线程正确接收WiFi数据帧"
    status: failed
    reason: "pcap句柄为nullptr，线程无法实际工作"
    artifacts:
      - path: "src/wfb_core.cpp"
        issue: "TODO注释显示pcap_handle = nullptr传递给线程函数"
    missing:
      - "创建实际的pcap句柄并传递给线程"
  - truth: "调度器线程在Server模式下正确运行"
    status: failed
    reason: "调度器使用存根实现，硬编码值而非实际调度逻辑"
    artifacts:
      - path: "src/scheduler_worker.cpp"
        issue: "TODO注释和硬编码的节点ID（node_id = 1）"
    missing:
      - "实现实际的调度器逻辑，调用get_next_node_to_serve()"
  - truth: "Token三连发正确执行（击穿机制）"
    status: failed
    reason: "调度器未实现Token发射逻辑"
    artifacts:
      - path: "src/scheduler_worker.cpp"
        issue: "缺少Token发射代码"
    missing:
      - "实现Token三连发逻辑"
  - truth: "pcap_inject失败有正确的错误处理和重试"
    status: partial
    reason: "TX线程框架存在，但未测试实际pcap_inject"
    artifacts:
      - path: "src/tx_worker.cpp"
        issue: "有错误处理代码，但未验证实际工作"
    missing:
      - "测试pcap_inject的实际错误处理"
  - truth: "队列达到水位线时停止读取，制造反压"
    status: partial
    reason: "水位线逻辑在单元测试中验证，但未在集成环境中测试"
    artifacts:
      - path: "src/tun_reader.cpp"
        issue: "有水位线检查代码，但未验证反压效果"
    missing:
      - "集成测试验证反压机制"
human_verification:
  - test: "验证wfb_core实际启动和运行"
    expected: "wfb_core --mode server -i wlan0 应启动而不崩溃，显示配置信息"
    why_human: "需要实际硬件接口和root权限"
  - test: "验证TUN设备正确创建"
    expected: "TUN设备应创建并配置txqueuelen=100"
    why_human: "需要root权限和网络接口操作"
  - test: "验证实时调度优先级设置"
    expected: "线程应正确设置SCHED_FIFO优先级（99/95/90）"
    why_human: "需要root权限和系统级验证"
  - test: "验证线程间通信和同步"
    expected: "线程应正确通信，condition_variable正常唤醒"
    why_human: "需要运行时监控和调试"
---

# Phase 5: 单进程 wfb_core 架构实现 验证报告

**阶段目标:** 实现真正的单进程 wfb_core 架构，替换多进程方案（wfb_tx/wfb_rx/wfb_tun）
**已验证:** 2026-04-28T23:30:00Z
**状态:** 发现缺口 (gaps_found)
**重新验证:** 否 - 初始验证

## 目标达成情况

### 可观察的真理

| # | 真理 | 状态 | 证据 |
|---|------|------|------|
| 1 | wfb_core进程能够成功启动并解析命令行参数 | ✗ 失败 | 编译错误：ServerScheduler构造函数问题 |
| 2 | Server和Client双模式入口正确切换 | ✓ 已验证 | wfb_core.cpp中有模式切换逻辑 |
| 3 | FEC参数通过命令行配置，不再硬编码 | ✓ 已验证 | config.h中FECConfig结构体可配置 |
| 4 | 所有系统调用失败都有明确的错误处理和重试逻辑 | ✓ 已验证 | error_handler.h中有RETRY_ON_ERROR宏 |
| 5 | 信号处理触发优雅退出，所有线程正常结束 | ✓ 已验证 | wfb_core.cpp中有signal_handler和线程join |
| 6 | RX线程正确接收WiFi数据帧 | ✗ 失败 | pcap句柄为nullptr |
| 7 | TokenFrame解析正确，状态更新正确 | ⚠️ 部分 | 有解析代码但未集成测试 |
| 8 | pcap句柄使用智能指针管理，无资源泄漏 | ⚠️ 部分 | resource_guard.h已定义，但使用不完整 |
| 9 | SCHED_FIFO 99优先级正确设置 | ✓ 已验证 | set_thread_realtime_priority调用 |
| 10 | 发射线程正确等待condition_variable唤醒（零轮询） | ✓ 已验证 | tx_worker.cpp中使用cv_send.wait |
| 11 | Token授权后，在窗口内正确发射数据包 | ⚠️ 部分 | 有框架但未测试实际发射 |
| 12 | Token过期时立即停止发射 | ⚠️ 部分 | 有检查逻辑但未测试 |
| 13 | pcap_inject失败有正确的错误处理和重试 | ⚠️ 部分 | 有错误处理框架但未验证 |
| 14 | SCHED_FIFO 90优先级正确设置 | ✓ 已验证 | set_thread_realtime_priority调用 |
| 15 | 帧构建内存安全，无缓冲区溢出 | ⚠️ 部分 | 有安全检查但未全面测试 |
| 16 | TUN设备正确创建并配置 | ⚠️ 部分 | tun_reader.cpp中有代码但未测试 |
| 17 | txqueuelen设置为100（极小水池反压） | ✓ 已验证 | tun_reader.cpp中设置txqueuelen |
| 18 | FEC编码器正确初始化和管理 | ⚠️ 部分 | 有zfex_code处理但未测试 |
| 19 | 队列达到水位线时停止读取，制造反压 | ⚠️ 部分 | 单元测试通过，未集成测试 |
| 20 | TUN文件描述符和FEC编码器使用RAII管理 | ⚠️ 部分 | resource_guard.h已定义，使用不完整 |
| 21 | 调度器线程在Server模式下正确运行 | ✗ 失败 | 使用存根实现，硬编码值 |
| 22 | Token三连发正确执行（击穿机制） | ✗ 失败 | 未实现Token发射逻辑 |
| 23 | 保护间隔（5ms）在节点切换时正确应用 | ✗ 失败 | 调度器未实现节点切换 |
| 24 | 所有Token发射错误都有处理和日志 | ⚠️ 部分 | 有错误处理框架但未实现 |
| 25 | SCHED_FIFO 95优先级正确设置 | ✓ 已验证 | set_thread_realtime_priority调用 |
| 26 | 节点状态机跃迁符合Phase 3设计 | ✗ 失败 | 调度器使用硬编码值 |
| 27 | server_start.sh正确启动wfb_core单进程 | ⚠️ 部分 | 脚本存在但wfb_core无法编译 |
| 28 | client_start.sh正确启动wfb_core单进程 | ⚠️ 部分 | 脚本存在但wfb_core无法编译 |
| 29 | PID文件正确写入和清理 | ✓ 已验证 | 启动脚本中有PID文件处理 |
| 30 | TUN设备在退出时正确删除 | ⚠️ 部分 | 有代码但未测试 |
| 31 | 向后兼容选项可用（LEGACY_MODE） | ✓ 已验证 | 启动脚本支持--legacy参数 |
| 32 | 所有单元测试通过（至少8个测试用例） | ✓ 已验证 | 10个测试用例全部通过 |
| 33 | 线程启动和退出正常 | ✓ 已验证 | 单元测试验证线程启动退出 |
| 34 | Token授权和过期逻辑正确 | ✓ 已验证 | 单元测试验证Token逻辑 |
| 35 | 队列反压机制正常工作 | ✓ 已验证 | 单元测试验证反压 |
| 36 | 动态水位线计算符合公式 | ✓ 已验证 | 单元测试验证水位线计算 |
| 37 | TokenFrame序列化/反序列化正确 | ✓ 已验证 | 单元测试验证序列化 |
| 38 | condition_variable唤醒机制正常 | ✓ 已验证 | 单元测试验证唤醒机制 |
| 39 | 集成测试脚本成功运行 | ✗ 失败 | 集成测试因编译失败而失败 |

**分数:** 20/39 真理已验证

### 所需工件

| 工件 | 预期 | 状态 | 详情 |
|------|------|------|------|
| `src/wfb_core.cpp` | 单进程入口，Server/Client双模式 | ✓ 存在 | 11806行，完全实现 |
| `src/config.h` | 配置结构体，FEC参数可配置 | ✓ 存在 | 包含FECConfig |
| `src/error_handler.h` | 错误处理宏和重试逻辑 | ✓ 存在 | 包含RETRY_ON_ERROR |
| `src/rx_demux.h/cpp` | RX线程实现 | ✓ 存在 | 6636行实现 |
| `src/tx_worker.h/cpp` | TX线程实现 | ✓ 存在 | 7031行实现 |
| `src/tun_reader.h/cpp` | TUN读取线程实现 | ✓ 存在 | 10217行实现 |
| `src/scheduler_worker.h/cpp` | 调度器线程实现 | ⚠️ 存根 | 有TODO注释，硬编码值 |
| `src/resource_guard.h` | RAII资源管理器 | ⚠️ 部分 | 定义类型但使用不完整 |
| `scripts/server_start.sh` | 服务端启动脚本 | ⚠️ 短 | 187行（需要200） |
| `scripts/client_start.sh` | 客户端启动脚本 | ⚠️ 短 | 194行（需要200） |
| `tests/test_wfb_core.cpp` | 单元测试套件 | ✓ 存在 | 12932行，10个测试 |
| `tests/integration_test_v2.sh` | 集成测试脚本 | ✓ 存在 | 3992行 |

### 关键链接验证

| 从 | 到 | 通过 | 状态 | 详情 |
|----|----|------|------|------|
| `src/wfb_core.cpp` | `src/config.h` | Config结构体 | ✓ 已连接 | 使用Config config |
| `src/wfb_core.cpp` | `src/error_handler.h` | 错误处理宏 | ✓ 已连接 | 使用set_error_callback |
| `src/rx_demux.cpp` | `src/resource_guard.h` | pcap句柄管理 | ⚠️ 部分 | 定义了PcapHandle但使用有限 |
| `src/tun_reader.cpp` | `src/resource_guard.h` | FEC编码器和TUN fd管理 | ✗ 未连接 | 未使用ZfexCodeHandle/FdHandle |
| `src/wfb_core.cpp` | 线程函数 | 线程启动 | ⚠️ 部分 | 启动但传递nullptr句柄 |

### 数据流追踪（第4级）

| 工件 | 数据变量 | 来源 | 产生真实数据 | 状态 |
|------|----------|------|--------------|------|
| `tx_worker.cpp` | can_send | TokenFrame解析 | ✗ 静态 | 有框架但数据源不完整 |
| `scheduler_worker.cpp` | node_id | 调度器 | ✗ 硬编码 | 硬编码为1 |

### 行为性抽查

| 行为 | 命令 | 结果 | 状态 |
|------|------|------|------|
| 编译wfb_core | `make wfb_core` | ServerScheduler构造函数错误 | ✗ 失败 |
| 运行单元测试 | `./test_wfb_core` | 10个测试全部通过 | ✓ 通过 |
| 检查帮助信息 | `./build/wfb_core --help` | 二进制文件不存在 | ✗ 失败 |

### 需求覆盖

| 需求 | 来源计划 | 描述 | 状态 | 证据 |
|------|----------|------|------|------|
| PROC-01 | 05-00 | 合并进程为单进程 | ✓ 满足 | wfb_core单进程实现 |
| PROC-02 | 05-00 | 控制面/数据面分流 | ⚠️ 部分 | 有框架但TODO |
| PROC-04 | 05-01 | RX线程优先级99 | ✓ 满足 | set_thread_realtime_priority调用 |
| PROC-06 | 05-01 | poll替代usleep | ⚠️ 部分 | 使用pcap_next_ex而非poll |
| PROC-07 | 05-02 | condition_variable替代usleep | ✓ 满足 | tx_worker.cpp中使用cv_send |
| RT-01 | 05-00 | 实时优先级函数 | ✓ 满足 | set_thread_realtime_priority实现 |
| RT-02 | 05-01 | RX线程优先级99 | ✓ 满足 | 正确设置 |
| RT-05 | 05-00 | Root权限检查 | ✓ 满足 | wfb_core.cpp中检查geteuid() |

### 反模式发现

| 文件 | 行 | 模式 | 严重性 | 影响 |
|------|----|------|--------|------|
| `src/wfb_core.cpp` | 167,195,301 | TODO: pcap句柄为nullptr | 🛑 阻塞 | 线程无法工作 |
| `src/scheduler_worker.cpp` | 87,148,154 | TODO: 调用调度器函数 | 🛑 阻塞 | 调度器不工作 |
| `src/wfb_core.cpp` | 143 | ServerScheduler构造函数错误 | 🛑 阻塞 | 编译失败 |
| `scripts/server_start.sh` | - | 187行（需要200） | ⚠️ 警告 | 次要问题 |
| `scripts/client_start.sh` | - | 194行（需要200） | ⚠️ 警告 | 次要问题 |

### 需要人工验证的项目

#### 1. 验证wfb_core实际启动和运行
**测试:** 运行 `wfb_core --mode server -i wlan0`（使用实际接口）
**预期:** 程序应启动而不崩溃，显示配置信息，等待信号退出
**需要人工原因:** 需要实际硬件接口和root权限，无法自动化测试

#### 2. 验证TUN设备正确创建
**测试:** 启动wfb_core后检查TUN设备 `ip link show wfb0`
**预期:** TUN设备应创建，txqueuelen=100，状态为UP
**需要人工原因:** 需要root权限和网络接口操作

#### 3. 验证实时调度优先级设置
**测试:** 使用 `ps -eo pid,rtprio,ni,cmd | grep wfb_core` 检查线程优先级
**预期:** 线程应显示正确的实时优先级（99/95/90）
**需要人工原因:** 需要root权限和系统级验证

#### 4. 验证线程间通信和同步
**测试:** 运行程序，发送Token，观察TX线程唤醒和发射
**预期:** 线程应正确通信，condition_variable正常唤醒，数据正确发射
**需要人工原因:** 需要运行时监控和调试，复杂状态机验证

### 缺口总结

Phase 5 在架构设计和单元测试方面取得了显著进展，但存在关键缺口阻碍目标达成：

**主要阻塞问题:**
1. **编译失败** - ServerScheduler构造函数问题阻止wfb_core编译
2. **pcap句柄缺失** - 线程接收nullptr，无法实际工作  
3. **调度器存根** - 使用硬编码值而非实际调度逻辑

**架构完整性:**
- 单进程架构正确实现 ✓
- 线程分离设计正确 ✓
- FEC参数可配置 ✓
- 错误处理框架完整 ✓
- 单元测试全面通过 ✓

**缺失的关键功能:**
- 实际的pcap句柄创建和传递
- 调度器实际逻辑实现
- Token发射机制
- 集成环境验证

**建议的修复优先级:**
1. 修复ServerScheduler构造函数问题（编译阻塞）
2. 实现pcap句柄创建和传递给线程
3. 完成调度器实际逻辑
4. 实现Token发射机制
5. 运行集成测试验证端到端功能

**结论:** 阶段目标"实现真正的单进程wfb_core架构" **未完全达成**。虽然架构基础牢固，单元测试全面，但关键运行时组件缺失，阻止了实际功能实现。

---

_已验证: 2026-04-28T23:30:00Z_
_验证器: Claude (gsd-verifier)_