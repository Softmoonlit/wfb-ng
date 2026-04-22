# 需求：联邦学习无线空口底层传输架构

**定义日期**: 2026-04-22
**核心价值**: 基于深缓冲区与动态时间配额轮调度的无碰撞传输架构，彻底消灭射频碰撞并逼近物理极限带宽利用率

---

## v1 需求

### 控制平面

- [ ] **CTRL-01**: 系统定义超轻量级 MAC 控制帧结构（TokenFrame），包含魔数（0x02）、目标节点、授权时间片、序列号
- [ ] **CTRL-02**: 实现 TokenFrame 的序列化和反序列化函数，确保字节对齐无填充
- [ ] **CTRL-03**: 服务器生成全局单调递增的序列号，防止 RX 缓冲区旧包诈尸
- [ ] **CTRL-04**: 控制帧使用 Radiotap 最低 MCS 速率模板封装，确保全网覆盖
- [ ] **CTRL-05**: 控制帧跳过 FEC 前向纠错编码，避免积压组块延迟
- [ ] **CTRL-06**: 发送 Token 时使用三连发击穿机制，接收端通过序列号自动去重
- [ ] **CTRL-07**: 服务器在节点切换时插入 5ms 保护间隔，确保上一节点硬件队列排空
- [ ] **CTRL-08**: 控制帧发射使用双 Radiotap 模板分流（最低 MCS + 高阶 MCS），零延迟切换

### 调度器（服务端）

- [ ] **SCHED-01**: 实现三态状态机引擎，支持状态跃迁：
  - 状态 I: 枯竭与睡眠（Zero-Payload Drop，< 5ms）
  - 状态 II: 唤醒与需量拟合（Demand-Fitting，5ms ≤ t < allocated）
  - 状态 III: 高负载贪突发（Additive Increase，t ≥ allocated）
- [ ] **SCHED-02**: 状态 I 引入 Watchdog 动量拦截，单次丢包不立即降级，连续 3 次静默才判定断流
- [ ] **SCHED-03**: 状态 II 实现需量拟合，next_window = max(MIN_WINDOW, actual_used + ELASTIC_MARGIN)，绝对不乘性减半
- [ ] **SCHED-04**: 状态 III 实现加法扩窗，next_window = min(MAX_WINDOW, allocated + STEP_UP)
- [ ] **SCHED-05**: MIN_WINDOW 设为 50ms，覆盖 OS 调度抖动（20ms）与侦听底线（30ms）
- [ ] **SCHED-06**: MAX_WINDOW 设为 300ms（10 节点）或 600ms（5 节点），确保 (N-1)×MAX_WINDOW < UFTP 断连超时
- [ ] **SCHED-07**: STEP_UP 设为 100ms，能在 2-3 个周期快速拉满占空比
- [ ] **SCHED-08**: ELASTIC_MARGIN 设为 50ms，包容 OS 调度毛刺与 UFTP 投递抖动
- [ ] **SCHED-09**: 实现空闲巡逻模式，全员深睡时仅探询 SQ 队首节点，长间隔 IDLE_PATROL_INTERVAL（100ms）
- [ ] **SCHED-10**: 实现混合交织调度，每服务 N 次 AQ 队列穿插 1 次 SQ 探询（INTERLEAVE_RATIO = 3-5）
- [ ] **SCHED-11**: 实现 AQ（活跃队列）和 SQ（睡眠队列）双队列管理，根据节点状态动态迁移
- [ ] **SCHED-12**: 实现下行广播"呼吸"周期：
  - 呼出 (800ms): 服务器全速下发 40MB 模型
  - 吸气 (200ms): 服务器暂停，依次分配 NACK 回传窗口（25ms/节点）
  - 循环执行直至 UFTP 确认 DONE
- [ ] **SCHED-13**: 实现高精度定时，使用 timerfd + 截止前极短时间空转自旋，确保微秒级绝对精度
- [ ] **SCHED-14**: 调度器主线程提权至 SCHED_FIFO 实时优先级（最高或次高）

### 缓冲与流控

- [ ] **BUFFER-01**: 实现线程安全的环形包队列（ThreadSafeQueue），静态分配 10000 包容量（约 15MB）
- [ ] **BUFFER-02**: 环形队列满时直接丢弃新包，触发快速反压
- [ ] **BUFFER-03**: 实现动态逻辑水位线计算函数，基于物理速率和授权时间动态计算队列限制
- [ ] **BUFFER-04**: 实现 MCS 速率映射表，C++ 代码解析启动参数映射实时物理速率（如 MCS 0 = 6Mbps）
- [ ] **BUFFER-05**: 客户端水位线公式：limit = ceil((R_phy × duration_ms × 1.5) / (8 × MTU × 1000))
- [ ] **BUFFER-06**: 服务端水位线公式：limit = ceil((R_phy × Inhale_Time × 1.5) / (8 × MTU))，其中 Inhale_Time = N × (NACK_WINDOW + GUARD_INTERVAL)
- [ ] **BUFFER-07**: 当内部队列积压达到动态水位线时，TUN 读取线程立即休眠停止调用 read()
- [ ] **BUFFER-08**: TUN 读取线程停止读网卡后，压力倒灌回操作系统 TUN 队列，引发内核队列溢出丢包（ENOBUFS）
- [ ] **BUFFER-09**: Linux TUN 网卡初始化时设置极小队列长度 txqueuelen = 100
- [ ] **BUFFER-10**: 极小 TUN 队列溢出丢包时，应用层 UFTP 收到 ENOBUFS 拥塞信号，触发退避

### 进程架构（客户端+服务端）

- [ ] **PROC-01**: 合并 wfb_tun（读取 TUN）和 wfb_tx（空口发射）为单进程，避免 UDP 通信环路丢包
- [ ] **PROC-02**: 实现控制面/数据面分流高速路由，独立线程处理
- [ ] **PROC-03**: 实现 TUN 读取线程（tun_reader_thread），普通优先级（CFS），负责读取、FEC 编码、制造反压
- [ ] **PROC-04**: 实现控制面接收线程（rx_demux_thread），最高实时优先级（SCHED_FIFO 99），负责极速抽干队列、解析 Token、唤醒发射端
- [ ] **PROC-05**: 实现空口发射线程（tx_main_loop），次高实时优先级（SCHED_FIFO 90），负责零轮询唤醒、逻辑阀门控制、pcap_inject
- [ ] **PROC-06**: 控制面接收线程使用 poll() 替代 usleep()，实现内核级硬件中断极速唤醒
- [ ] **PROC-07**: 发射端线程使用 std::condition_variable 替代 usleep()，实现零轮询延迟唤醒
- [ ] **PROC-08**: 实现全局原子变量 can_send（逻辑原子阀门）、token_expire_time_ms（Token 过期时间）、dynamic_queue_limit（动态水位线）
- [ ] **PROC-09**: TUN 读取线程检查 dynamic_queue_limit，达到后停止读取并 usleep(1000) 退避让出 CPU
- [ ] **PROC-10**: 发射端线程在 condition_variable 唤醒时立即释放锁，准备开火
- [ ] **PROC-11**: 发射端线程检查 can_send 和 token_expire_time_ms，不满足条件时不开火并设置 can_send = false
- [ ] **PROC-12**: TUN 读取线程在装弹后立即调用 cv_send.notify_one()，唤醒可能因缺弹而沉睡的射手

### 实时调度

- [ ] **RT-01**: 实现提权函数 set_thread_realtime_priority()，使用 pthread_setschedparam 设置 SCHED_FIFO 策略
- [ ] **RT-02**: 控制面接收线程提权至 SCHED_FIFO 优先级 99（最高）
- [ ] **RT-03**: 空口发射线程提权至 SCHED_FIFO 优先级 90（次高）
- [ ] **RT-04**: TUN 读取线程保留普通优先级（CFS），避免 FEC 编码（CPU 密集型）设为实时优先级导致系统死机
- [ ] **RT-05**: 启动时检测 Root 权限，权限不足时明确警告 "Must run as root to set SCHED_FIFO"
- [ ] **RT-06**: 使用 timerfd_create() 创建高精度定时器文件描述符
- [ ] **RT-07**: 使用 clock_gettime() 获取纳秒级时间戳
- [ ] **RT-08**: 实现 5ms 保护间隔的高精度休眠：timerfd + 极短时间（< 100μs）空转自旋，避免上下文切换开销

### 应用层集成

- [ ] **APP-01**: 提供 UFTP 微超发限速配置指南，限速 = (底层物理速率 / 节点总数) × 1.2
- [ ] **APP-02**: 提供 UFTP 超时阈值调整建议，适度调大 GRTT 和重传尝试次数，容忍秒级合理波动
- [ ] **APP-03**: 确认 UFTP 无需修改源码，继续使用标准 UDP/IP 协议
- [ ] **APP-04**: 验证应用层对底层调度零感知，通过标准网络接口（如 eth0）通信

---

## v2 需求

 deferred to future release

### 高级调度策略

- **SCHED-ADV-01**: 支持加权的混合交织调度（活跃节点按优先级服务）
- **SCHED-ADV-02**: 支持预测性窗口分配（基于历史训练时间）
- **SCHED-ADV-03**: 支持自适应 INTERLEAVE_RATIO（根据 AQ/SQ 动态调整 N）

### 性能优化

- **PERF-ADV-01**: FEC 编码使用 SSE/AVX 指令优化，提升 CPU 效率
- **PERF-ADV-02**: 控制帧批量发送优化，减少 pcap_inject 调用开销
- **PERF-ADV-03**: 网卡 DMA 传输优化，减少 CPU 拷贝

### 可观测性增强

- **OBS-ADV-01**: 集成 Prometheus 指标导出（队列深度、Token 授权时间、状态机跃迁）
- **OBS-ADV-02**: 实现实时 Web UI 显示调度状态和节点拓扑
- **OBS-ADV-03**: 集成分布式追踪系统（Jaeger/Zipkin），跟踪端到端延迟

### 多环境支持

- **ENV-ADV-01**: 支持 5GHz 和 2.4GHz 双频段 WiFi
- **ENV-ADV-02**: 支持 802.11ax/wifi6 新协议
- **ENV-ADV-03**: 支持容器化部署（Docker/K8s）

---

## Out of Scope

| 功能 | 原因 |
|------|------|
| 实现新的物理层调制解调方案 | 基于现有 wfb-ng 架构改造，保持兼容性 |
| 移除 FEC 前向纠错机制 | 保留用于数据帧，仅控制帧跳过 |
| 使用 Python/Go 等带 GC 的语言编写调度器 | 必须使用纯 C/C++ 消除运行时抖动毛刺 |
| 改用 Unix Socket 通信 | 继续使用标准 UDP/IP 协议，对应用层零感知 |
| 宏观同步 TDMA（微秒级） | 毫秒级容错的 Token Passing 足够，Linux 非实时内核无法保障 |
| 应用层感知底层调度 | 跨层耦合导致灵活性差，UFTP 继续使用 UDP/IP |
| 加密/认证机制 | 应用层业务逻辑，底层透明传输 |
| 负载均衡多服务器 | v1 单服务器架构，v2 考虑 |
| 实时内核补丁 | 依赖特定内核版本，可移植性差 |

---

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| CTRL-01, CTRL-02, CTRL-03 | Phase 1 | Pending |
| CTRL-04, CTRL-05, CTRL-06, CTRL-07, CTRL-08 | Phase 1 | Pending |
| SCHED-01 ~ SCHED-14 | Phase 3 | Pending |
| BUFFER-01 ~ BUFFER-10 | Phase 2 | Pending |
| PROC-01 ~ PROC-12 | Phase 2 | Pending |
| RT-01 ~ RT-08 | Phase 2 | Pending |
| APP-01 ~ APP-04 | Phase 4 | Pending |

**Coverage**:
- v1 需求: 51 总数
- 映射到阶段: 51
- 未映射: 0 ✓

---
*需求定义: 2026-04-22*
*Last updated: 2026-04-22 after initial definition*
