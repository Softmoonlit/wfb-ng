# Architecture: 联邦学习无线空口底层传输架构

**定义日期**: 2026-04-22

## 系统概览

本系统采用严格的三层网络栈架构，各层职责清晰，严禁跨层耦合。

```
┌─────────────────────────────────────────────────────────────┐
│                   应用层 (Application)                  │
│                  UFTP 零感知业务逻辑                    │
│                   UDP/IP 组播通信                       │
└────────────────────────┬────────────────────────────────────┘
                     │ 标准网络接口
                     ▼
┌─────────────────────────────────────────────────────────────┐
│               操作系统层 (Operating System)              │
│           Linux TUN 虚拟网卡 (wfb0)                  │
│         极小水池防抖动 (txqueuelen = 100)             │
└────────────────────────┬────────────────────────────────────┘
                     │ TUN 设备接口 (/dev/net/tun)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│        MAC/物理层 (MAC/Physical Layer)                  │
│              魔改 wfb-ng 单进程架构                    │
└─────────────────────────────────────────────────────────────┘
```

## 组件分解

### 1. 应用层：UFTP 零感知

**职责**：
- 按序、可靠地组播下发/收集 40MB 模型文件
- 适配联邦学习非对称性（1 服务器对 10 客户端）

**实现要点**：
- **无需修改 UFTP 源码**，继续使用标准 UDP/IP
- **微超发限速**: `-R` 参数设置为 `(物理速率 / 节点数) × 1.2`
- **超时阈值调整**: 适度调大 GRTT 和重传次数，容忍秒级波动

**边界**：
- **输入**: 标准网络接口（如 `eth0`）
- **输出**: TUN 设备（`wfb0`）
- **协议**: UDP/IP（对底层零感知）

**数据流**：
```
UFTP 进程 → 标准网络栈 → UDP 包 → TUN 设备 (wfb0)
                                                    ↓
                                              底层读取线程
                                                    ↓
                                              FEC 编码 + 控制逻辑
                                                    ↓
                                            空口发射线程
                                                    ↓
                                              WiFi Monitor 模式
```

### 2. 操作系统层：Linux TUN 极小水池

**职责**：
- 进程内核交互的微减震器
- 确保存取平滑，传递满载反压

**配置**：
- **txqueuelen = 100**: 仅保留微小缓冲隔离
- **快速溢出丢包**: 100 个包满后立即丢弃，触发 ENOBUFS

**原理（丢包自适应反压）**：
```
UFTP 注水速率 = 底层泄洪速率 × 1.2 (微超发)
        ↓
    填满用户态浅水池
        ↓
  TUN 读取线程停止 (水位线触发)
        ↓
  TUN 队列瞬间填满 (100 包)
        ↓
    内核队列溢出 (ENOBUFS)
        ↓
UFTP 收到拥塞信号 → 立即退避
```

**边界**：
- **输入**: 来自 TUN 设备的 IP 包（应用层流量）
- **输出**: 到 TUN 设备的 IP 包（从空口接收的 FEC 解码后数据）
- **控制**: 动态队列深度，基于水位线触发读取停顿

### 3. MAC/物理层：魔改 wfb-ng

**职责**：
- 彻底接管物理发射权（无 Token 不发射）
- 无碰撞调度中心控制点

**架构改造**：
```
原版架构 (双进程):
  wfb_tun (读取 TUN) → UDP Socket (127.0.0.1) → wfb_tx (发射)
  问题: UDP 缓冲区满 → 丢包 → 数据静默丢失

改造架构 (单进程):
  +---------------------------+
  |      wfb-ng 单进程       |
  |  ┌───────────────────┐  |
  |  │  物理内存池        │  |  ← 10000 包，约 15MB
  |  └───────────────────┘  |
  |           ↓            |
  |  ┌───────────────────┐  |
  |  │  逻辑水位线        │  |  ← 动态计算，原子变量
  |  └───────────────────┘  |
  |           ↓            |
  |  ┌───────────────────┐  |
  |  │  控制面/数据面分流  │  |
  |  └───────────────────┘  |
  |       ↓     ↓       ↓   |
  |    rx_tun  tx_loop  ... |
  +---------------------------+
```

**核心线程**：

#### 线程 A: TUN 读取线程（装弹手）
```cpp
void tun_reader_thread() {
    // 普通优先级（CFS），CPU 密集型 FEC 编码
    while (1) {
        if (internal_queue.size() >= dynamic_queue_limit) {
            usleep(1000);  // 停顿，让压力倒灌
            continue;
        }
        Packet pkt = read_from_tun();
        FecPacket fec_pkt = encode_fec(pkt);
        internal_queue.push(fec_pkt);
        cv_send.notify_one();  // 唤醒发射端
    }
}
```

**优先级**: 普通优先级（CFS）
**原因**: FEC 编码是纯 CPU 密集型，设为实时优先级会导致系统死机
**容差**: 1.5 倍底仓容忍调度延迟

#### 线程 B: 控制面接收线程（哨兵）
```cpp
void* rx_demux_thread() {
    // 最高实时优先级（SCHED_FIFO 99）
    set_thread_realtime_priority(99);

    while (1) {
        poll(raw_sock_fd, -1);  // 内核级硬件中断极速唤醒

        // 极速抽干队列（非阻塞）
        while (recv_from_air_nonblocking(&raw_pkt) > 0) {
            if (is_control_frame(raw_pkt)) {
                TokenFrame token = parse_token(raw_pkt);
                if (token.seq_num > max_seen_seq) {
                    max_seen_seq = token.seq_num;
                    latest_token = token;
                    got_new_token = true;
                }
            } else if (is_data_frame(raw_pkt)) {
                FecPacket fec_pkt = decode_fec(raw_pkt);
                if (fec_is_valid(fec_pkt)) {
                    write_to_tun(wfb_tun_fd, fec_pkt.ip_payload);
                }
            }
        }

        // 处理最新 Token
        if (got_new_token) {
            if (latest_token.target_node == MY_NODE_ID) {
                token_expire_time_ms.store(current_time + duration);
                {
                    lock_guard<mutex> lk(cv_m);
                    can_send.store(true);
                }
                cv_send.notify_one();

                size_t new_limit = calculate_dynamic_limit(...);
                dynamic_queue_limit.store(new_limit);
            } else {
                can_send.store(false);
            }
        }
    }
}
```

**优先级**: 最高实时优先级（SCHED_FIFO 99）
**原因**: 控制帧实时性要求最高，必须第一时间响应
**优化**:
- `poll()` 替代 `usleep()`，内核级中断唤醒
- 非阻塞一次性抽干队列，避免循环开销

#### 线程 C: 空口发射线程（射手）
```cpp
void tx_main_loop() {
    // 次高实时优先级（SCHED_FIFO 90）
    set_thread_realtime_priority(90);

    while (1) {
        // 零轮询唤醒
        unique_lock<mutex> lk(cv_m);
        cv_send.wait(lk, []{
            return (can_send && !internal_queue.empty()) ||
                   (current_time > token_expire_time_ms);
        });
        lk.unlock();

        // 逻辑原子阀门
        if (!can_send || current_time > token_expire_time_ms) {
            can_send.store(false);
            continue;
        }

        // 直接注入射频包
        FecPacket fec_pkt = internal_queue.pop();
        pcap_inject(fec_pkt);
    }
}
```

**优先级**: 次高实时优先级（SCHED_FIFO 90）
**原因**: 射频发射需要精确控制，但不能抢占控制面
**优化**:
- `condition_variable` 替代 `usleep()`，消除 1-5ms 毛刺
- 原子变量 `can_send` 确保无锁快速查询

## 数据流图

### 下行数据流（服务器 → 客户端）
```
1. UFTP 服务器端读取 40MB 模型文件
   ↓
2. 通过标准网络栈发送到 eth0
   ↓
3. TUN 设备 (wfb0) 接收到 IP 包
   ↓
4. tun_reader_thread 读取并 FEC 编码
   ↓
5. 存入 internal_queue (物理内存池)
   ↓
6. tx_main_loop 等待发射令牌
   ↓
7. 收到 Token (target_node = MY_ID 或 ALL)
   ↓
8. 唤醒 tx_main_loop，清空队列发送
   ↓
9. pcap_inject() 注入到 WiFi Monitor 模式
   ↓
10. 客户端接收并解码
   ↓
11. 写入客户端 TUN 设备
   ↓
12. UFTP 客户端端接收 IP 包
```

### 上行数据流（客户端 → 服务器）
```
1. UFTP 客户端端收集本地训练梯度
   ↓
2. 通过标准网络栈发送到 eth0
   ↓
3. TUN 设备 (wfb0) 接收到 IP 包
   ↓
4. tun_reader_thread 读取并 FEC 编码
   ↓
5. 存入 internal_queue
   ↓
6. 等待 Token 授权
   ↓
7. rx_demux_thread 收到 Token
   ↓
8. 更新 token_expire_time_ms，唤醒 tx_main_loop
   ↓
9. tx_main_loop 清空队列发送
   ↓
10. 服务器接收并解码
   ↓
11. 写入服务器 TUN 设备
   ↓
12. UFTP 服务器端接收 IP 包
```

### 控制帧流（服务器 → 客户端）
```
1. Server Scheduler 主循环
   ↓
2. 生成 TokenFrame (magic, target_node, duration_ms, seq_num)
   ↓
3. 使用最低 MCS Radiotap 模板封装
   ↓
4. 调用 pcap_inject() 三次（三连发击穿）
   ↓
5. 客户端 rx_demux_thread 接收（poll 唤醒）
   ↓
6. 校验魔数和序列号
   ↓
7. 更新本地状态（can_send, token_expire_time_ms, watermark）
   ↓
8. 唤醒 tx_main_loop 发射数据
```

## 核心数据结构

### TokenFrame（控制帧）
```cpp
struct TokenFrame {
    uint8_t  magic;       // 0x02，区分控制/数据帧
    uint8_t  target_node; // 目标节点 (0=广播/ALL)
    uint16_t duration_ms; // 授权时间片 (毫秒)
    uint32_t seq_num;     // 全局单调递增序号
};
#pragma pack(push, 1)  // 字节对齐，无填充
#pragma pack(pop)
```

**特性**：
- 超轻量级（8 字节）
- 无 FEC 编码
- 三连发送可靠性保障
- 序列号防旧包诈尸

### ThreadSafeQueue（物理内存池）
```cpp
template<typename T>
class ThreadSafeQueue {
private:
    vector<T> buffer;
    size_t max_capacity;
    size_t head, tail, count;
    mutex mtx;

public:
    ThreadSafeQueue(size_t capacity) : max_capacity(capacity), head(0), tail(0), count(0) {
        buffer.resize(capacity);  // 静态分配，杜绝 bad_alloc
    }

    void push(const T& item) {
        lock_guard<mutex> lock(mtx);
        if (count == max_capacity) return;
        buffer[tail] = item;
        tail = (tail + 1) % max_capacity;
        count++;
    }

    T pop() {
        lock_guard<mutex> lock(mtx);
        if (count == 0) throw runtime_error("队列为空");
        T item = buffer[head];
        head = (head + 1) % max_capacity;
        count--;
        return item;
    }

    size_t size() {
        lock_guard<mutex> lock(mtx);
        return count;
    }
};
```

**特性**：
- 静态分配 10000 包容量（约 15MB）
- 环形缓冲区，避免拷贝
- 线程安全（mutex 保护）
- 满时丢弃，快速反压

### 全局原子变量
```cpp
std::atomic<bool> can_send{false};
std::atomic<uint64_t> token_expire_time_ms{0};
std::atomic<size_t> dynamic_queue_limit{3000};
static uint32_t max_seen_seq = 0;
std::condition_variable cv_send;
std::mutex cv_m;
```

**用途**：
- `can_send`: 逻辑原子阀门，无锁快速查询
- `token_expire_time_ms`: Token 过期时间，精确控制发射
- `dynamic_queue_limit`: 动态水位线，自适应背压
- `max_seen_seq`: 序列号去重，防旧包诈尸
- `cv_send + cv_m`: 零轮询唤醒，消除毛刺

## 调度器架构（服务端）

### 主状态机
```
┌─────────────────────────────────────────────────────────┐
│              Server Scheduler 主循环                  │
└─────────────────────────────────────────────────────────┘
         ↓
    ┌────┴────┐
    ↓         ↓         ↓
相位一    相位二     相位三
(广播)   (探询)    (泄洪)
    ↓         ↓         ↓
"呼吸"    交织      弹性
周期      调度      扩窗
```

### 相位一：下行广播与"呼吸"反馈
```
呼出 (800ms):
  ├── Token: target_node = ALL, duration = 800ms
  ├── 发射逻辑阀门全开
  └── 客户端全速接收模型

吸气 (200ms):
  ├── 暂停下发 (can_send = false)
  ├── 依次给每个客户端发 Token (NACK_WINDOW = 25ms)
  ├── 间隔: GUARD_INTERVAL = 5ms
  └── 客户端回传 NACK 数据包
```

**循环**: 直至 40MB 下行完成且 UFTP 确认 DONE

### 相位二：分散交织探询（AEW）
```
双队列:
  ├── Active Queue (AQ): 正在回传数据的节点
  └── Sleep Queue (SQ): 训练中/无数据的节点

场景 1: 全员深睡 (AQ 为空)
  └── 空闲巡逻模式
      ├── 每次从 SQ 抽 1 个节点
      ├── Token: duration = 15ms (微探询)
      ├── 间隔: IDLE_PATROL_INTERVAL = 100ms
      └── 无响应则放回 SQ 队尾

场景 2: 混合调度 (AQ + SQ)
  └── 交织调度 (INTERLEAVE_RATIO = N=3-5)
      ├── 连续服务 AQ 队列 N 次
      │   ├── Token: duration = 动态 (50-300ms)
      │   ├── 插入 5ms 保护间隔
      │   └── 根据实际使用时间计算下一轮窗口
      ├── 穿插 1 次 SQ 探询
      │   └── Token: duration = 15ms
      └── 循环往复
```

### 三态状态机（动态授权规则）
```
每次收集到 actual_used_time 后执行:

状态 I: 枯竭与睡眠 (Zero-Payload Drop)
  条件: actual_used_time < 5ms
  └── Watchdog 拦截
      ├── 若上一轮状态 III 且此次静默 → 保持窗口（丢包误判）
      ├── 若连续 3 次静默 → 降级为 MIN_WINDOW (50ms)
      └── 从 AQ 踢出，放入 SQ 队尾

状态 II: 唤醒与需量拟合 (Demand-Fitting)
  条件: 5ms <= actual_used_time < allocated_window
  └── 立刻拔出 SQ，插入 AQ 队尾
      ├── next_window = max(MIN_WINDOW, actual_used_time + ELASTIC_MARGIN)
      ├── 绝不乘性减半
      └── 清零沉默计数器

状态 III: 高负载贪突发 (Additive Increase)
  条件: actual_used_time >= allocated_window
  └── 必在 AQ 中
      ├── next_window = min(MAX_WINDOW, allocated_window + STEP_UP)
      └── STEP_UP = 100ms，2-3 周期拉满
```

## 线程调度与优先级

### 优先级分配
```
优先级 99 (SCHED_FIFO)  → 控制面接收线程
优先级 90 (SCHED_FIFO)  → 空口发射线程
优先级 0 (CFS)         → TUN 读取线程 + 主循环
```

### 原因
- **控制面最高**: 必须第一时间响应 Token，否则错过授权窗口
- **发射端次高**: 需要精确控制发射，但不能抢占控制面
- **读取线程普通**: FEC 编码是 CPU 密集型，实时优先级会导致死机

### 实时调度函数
```cpp
void set_thread_realtime_priority(int priority) {
    pthread_t this_thread = pthread_self();
    struct sched_param params;
    params.sched_priority = priority;
    if (pthread_setschedparam(this_thread, SCHED_FIFO, &params) != 0) {
        printf("Warning: Must run as root for SCHED_FIFO!\n");
    }
}
```

## 关键时间参数

### 物理层时间（毫秒级容错）
| 参数 | 值 | 用途 |
|------|-----|------|
| MIN_WINDOW | 50ms | 覆盖 OS 调度抖动（20ms）+ 侦听底线（30ms） |
| MAX_WINDOW | 300ms (10 节点) / 600ms (5 节点) | 确保 (N-1)×MAX_WINDOW < UFTP 断连超时 |
| STEP_UP | 100ms | 2-3 周期拉满占空比 |
| ELASTIC_MARGIN | 50ms | 包容系统抖动和投递毛刺 |
| MICRO_PROBE_DURATION | 15ms | 覆盖调度抖动 + 网卡延迟 |
| GUARD_INTERVAL | 5ms | 节点切换保护，避免空中撞车 |
| IDLE_PATROL_INTERVAL | 100ms | 消除探询风暴，低频谱占用 |

### 高精度定时（微秒级）
- **timerfd_create()**: 创建定时器文件描述符
- **clock_gettime()**: 获取纳秒级时间戳
- **空转自旋**: 截止前极短时间（< 100μs）自旋，消除上下文切换开销

## 构建顺序建议

### 第一阶段：基础框架
1. 控制帧结构及序列化（TokenFrame）
2. 线程安全环形队列（ThreadSafeQueue）
3. 动态水位线计算函数（calculate_dynamic_limit）

### 第二阶段：客户端核心
4. 客户端进程合并（wfb_tun + wfb_tx）
5. 控制面/数据面分流线程
6. 实时调度提权（SCHED_FIFO）

### 第三阶段：服务端调度
7. 三态状态机引擎（ServerScheduler）
8. 空闲巡逻模式实现
9. 混合交织调度实现
10. 下行呼吸周期实现

### 第四阶段：集成优化
11. 控制帧 Radiotap 模板分流
12. 三连发击穿机制
13. MCS 速率映射表集成
14. UFTP 限速配置指南
15. 端到端集成测试

## 错误处理与恢复

### 数据包丢失
- **原因**: 无线干扰、硬件故障
- **处理**: UFTP 超时重传，底层透明

### Token 丢失
- **原因**: 无线干扰、三连发全部失败
- **处理**: Watchdog 保护，不立即降级，连续 3 次静默后才判定断流

### 内存溢出
- **原因**: 队列配置过小
- **处理**: ThreadSafeQueue 满时直接丢弃，触发快速反压

### 线程阻塞
- **原因**: 死锁、优先级反转
- **处理**: 使用 `condition_variable` 避免忙等，限时等待 + 超时重试

## 性能瓶颈分析

### CPU
- **FEC 编码**: CPU 密集型，使用优化库（如 SSE 指令）
- **加密/解密**（如启用）: 使用硬件加速（AES-NI）

### 内存
- **队列容量**: 10000 包约 15MB，根据实际调整
- **拷贝开销**: 零拷贝设计（引用计数、智能指针）

### 网络
- **Monitor 模式**: 绕过内核网络栈，libpcap 直接注入
- **Radiotap 头**: 预生成模板，避免每次构造

### 磁盘
- **日志**: 异步写入，避免阻塞主线程
- **数据文件**: 内存映射（mmap）零拷贝读取

## 可扩展性设计

### 节点数扩展
- **配置化**: MAX_WINDOW, IDLE_PATROL_INTERVAL 可参数化
- **自适应**: 根据在线节点数动态调整调度策略

### 算法插件化
- **调度策略**: 可插拔，支持不同算法（公平、优先级、加权的）
- **状态机**: 可配置跃迁规则，适应不同场景

### 协议扩展
- **控制帧**: 预留字段，未来可扩展（如带宽请求、QoS 标记）
- **版本兼容**: 帧内版本号字段，支持协议演进

---

*Research generated: 2026-04-22*
