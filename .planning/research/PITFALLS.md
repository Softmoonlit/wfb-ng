# Pitfalls: 联邦学习无线空口底层传输架构

**定义日期**: 2026-04-22

## 陷阱概览

这些陷阱是联邦学习无线空口传输项目中最常见且致命的错误，必须在设计和实现中提前防范。

---

## 陷阱 1: 双进程通信环路数据静默丢失

### 描述
原版 wfb-ng 架构中，`wfb_tun`（读取 TUN）和 `wfb_tx`（空口发射）是独立进程，通过本地 UDP Socket（127.0.0.1）通信。当 `wfb_tx` 逻辑阀门关闭时（如吸气期停火），`wfb_tun` 仍疯狂抽干 TUN 队列塞向 UDP。一旦 UDP 接收缓冲区（`rmem`）打满，内核直接丢弃后续包（ENOBUFS）。

### 后果
- **数据根本憋不住**，直接在内部通信环路丢失
- **UFTP 不会收到任何拥塞信号**，持续发送直到缓冲区溢出
- **应用层误以为发送成功**，实际数据已丢失

### 预警信号
- 发射端阀门关闭时，TUN 读取线程仍持续读取
- UDP Socket 接收缓冲区持续增长
- 网络抓包显示数据包消失（未到达空口）

### 防范策略
**必须合并进程**：将 `wfb_tun` 的读取逻辑与 `wfb_tx` 的发射逻辑合并到同一进程中。

**实现方式**：
```cpp
// 单进程内物理内存池
ThreadSafeQueue<Packet> internal_queue(10000);

// 线程间直接内存通信，无 UDP 中转
void tun_reader_thread() {
    // 直接存入 internal_queue
    internal_queue.push(encode_fec(read_from_tun()));
}

void tx_main_loop() {
    // 直接从 internal_queue 取出
    pcap_inject(internal_queue.pop());
}
```

**验证方式**：在发射阀门关闭时，TUN 读取线程应停止调用 `read()`，可通过日志确认。

---

## 陷阱 2: 水位线硬编码导致 Bufferbloat

### 描述
动态逻辑水位线 `dynamic_queue_limit` 若硬编码为固定值（如 3000 包），当底层物理速率变化时（MCS 自动降级或升级），水位线不再适配。例如：硬编码假设 100Mbps，实际仅 6Mbps 时，算出的 3000 包需要数秒才能排空，导致灾难性 Bufferbloat。

### 后果
- **排队延迟不可控**，可能达到数十秒
- **UFTP 超时假死**，状态机崩溃
- **应用层误判为网络故障**，触发不必要重传

### 预警信号
- 网络流量图显示队列深度周期性暴涨
- UFTP 日志频繁出现超时（Timeout）
- 客户端在训练完成很久后才开始回传（数据积压在队列中）

### 防范策略
**MCS 动态映射表**：C++ 代码必须解析启动参数传入的 MCS 和频宽，通过内置查找表映射出实时物理速率。

**实现方式**：
```cpp
uint64_t get_phy_rate_bps(uint8_t mcs, uint8_t bandwidth_mhz) {
    // 内置查找表
    static const struct { uint8_t mcs, bw; uint64_t rate; } table[] = {
        {0, 20, 6'000'000},   // MCS 0, 20MHz = 6Mbps
        {1, 20, 9'000'000},   // MCS 1, 20MHz = 9Mbps
        {4, 20, 24'000'000},  // MCS 4, 20MHz = 24Mbps
        // ... 完整表 ...
        {8, 40, 144'000'000}, // MCS 8, 40MHz = 144Mbps
    };

    for (auto& entry : table) {
        if (entry.mcs == mcs && entry.bw == bandwidth_mhz) {
            return entry.rate;
        }
    }
    return 6'000'000; // 默认最小速率
}

// 动态计算水位线
size_t calculate_dynamic_limit(uint16_t duration_ms) {
    uint64_t phy_rate = get_phy_rate_bps(current_mcs, current_bandwidth);
    double limit = (phy_rate * duration_ms * 1.5) / (8.0 * MTU * 1000.0);
    return static_cast<size_t>(std::ceil(limit));
}
```

**验证方式**：
- 日志中打印 `phy_rate_bps` 和计算的 `dynamic_queue_limit`
- 切换信道质量（改变 MCS），观察水位线是否动态调整

**应在哪个阶段解决**：Phase 2 - 实现动态逻辑水位线计算机制

---

## 陷阱 3: 实时调度未提权导致毛刺失控

### 描述
Linux 默认使用 CFS（完全公平调度器），线程优先级相同，无法保证实时线程优先执行。如果调度器主线程、控制面接收线程、发射端线程不提权至 `SCHED_FIFO`，系统调度抖动可达 10-50ms，导致 Token 发射精度完全丧失。

### 后果
- **Token 发射时间不确定**，早发或晚发
- **节点授权窗口重叠**，导致空中撞车（碰撞）
- **状态机跃迁失效**，调度器逻辑混乱

### 预警信号
- Wireshark 抓包显示 Token 发射间隔抖动极大（如 20ms ± 15ms）
- 服务器日志显示实际授权时间和预期不符
- 客户端报告 Token 序列号乱序或重复

### 防范策略
**必须提权至 SCHED_FIFO**：调度器主线程、控制面线程、发射端线程均需实时优先级。

**实现方式**：
```cpp
void set_thread_realtime_priority(int priority) {
    pthread_t this_thread = pthread_self();
    struct sched_param params;
    params.sched_priority = priority;

    if (pthread_setschedparam(this_thread, SCHED_FIFO, &params) != 0) {
        perror("pthread_setschedparam");
        printf("Warning: Must run as root for SCHED_FIFO!\n");
        exit(1);
    }
}

// 线程创建时提权
void* rx_demux_thread() {
    set_thread_realtime_priority(99); // 最高优先级
    // ... 主逻辑 ...
}

void* tx_main_loop() {
    set_thread_realtime_priority(90); // 次高优先级
    // ... 主逻辑 ...
}
```

**验证方式**：
- 检查 `/proc/<pid>/task/<tid>/sched` 文件，确认策略为 `FIFO`
- 使用 `chrt -p <tid>` 查看优先级
- 抓包统计 Token 发射时间间隔，标准差应 < 1ms

**应在哪个阶段解决**：Phase 1 - MAC 控制帧结构及序列化（涉及线程设计）

---

## 陷阱 4: usleep() 导致灾难性轮询延迟

### 描述
使用 `usleep()` 等待条件满足时，最小粒度受系统调度器限制（通常 1ms），且醒来时需要上下文切换，累计抖动可达 5-10ms。在 25ms 的微探询窗口中，5ms 抖动意味着 20% 的授权时间被浪费。

### 后果
- **Token 发射延迟不可控**，影响调度精度
- **零轮询唤醒机制失效**，无法做到即时响应
- **系统资源浪费**，CPU 频繁唤醒休眠

### 预警信号
- `usleep()` 调用后，实际休眠时间远超预期（如 `usleep(100)` 实际休眠 110ms）
- 线程唤醒后需要额外时间进入就绪状态
- 系统调用频繁，上下文切换开销大

### 防范策略
**使用条件变量（std::condition_variable）替代 usleep()**：实现零轮询唤醒，消除 1-5ms 毛刺。

**实现方式**：
```cpp
std::condition_variable cv_send;
std::mutex cv_m;

// 等待线程
void wait_thread() {
    std::unique_lock<std::mutex> lk(cv_m);
    cv_send.wait(lk, []{
        return (can_send.load() && !internal_queue.empty()) ||
               (get_current_time_ms() > token_expire_time_ms.load());
    });
    lk.unlock(); // 立刻释放锁，准备开火
}

// 通知线程
void notify_thread() {
    {
        std::lock_guard<std::mutex> lk(cv_m);
        can_send.store(true);
    }
    cv_send.notify_one(); // 零轮询延迟唤醒
}
```

**配合高精度定时**：
```cpp
#include <sys/timerfd.h>

int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
struct itimerspec its;
its.it_value.tv_sec = 0;
its.it_value.tv_nsec = 100'000'000; // 100ms
its.it_interval.tv_sec = 0;
its.it_interval.tv_nsec = 0;
timerfd_settime(timerfd, 0, &its, NULL);

// epoll/poll 等待 timerfd 可读
```

**验证方式**：
- 计时测量：记录 `cv_send.wait()` 到 `tx_main_loop()` 实际执行的时间差
- 应 < 100μs（微秒级），而非 1-5ms

**应在哪个阶段解决**：Phase 1 - MAC 控制帧结构及序列化（涉及线程通信设计）

---

## 陷阱 5: TUN 队列设置过大导致反压失效

### 描述
Linux TUN 设备的 `txqueuelen` 控制内核队列深度，默认值可能高达 1000+ 包。如果设置过大（如 `txqueuelen 1000`），即使应用层水位线触发停止读取，TUN 队列仍能容纳大量包，导致 UFTP 长时间收不到 `ENOBUFS` 拥塞信号。

### 后果
- **反压传递延迟**，UFTP 持续发送而不退避
- **Bufferbloat 复发**，排队延迟不可控
- **应用层误判网络容量充足**，过度发包

### 预警信号
- `ifconfig wfb0` 显示 `TX packets` 远大于 `RX packets`（积压）
- UFTP 发送速率远超物理速率
- 网络延迟测试显示排队时间 > 10s

### 防范策略
**设置极小 TUN 队列**：`ifconfig wfb0 txqueuelen 100`（恒定值）。

**实现方式**：
```bash
# 在 TUN 设备初始化脚本中
ip tuntap add dev wfb0 mode tun
ip link set wfb0 up
ifconfig wfb0 txqueuelen 100  # 极小队列，快速触发丢包
```

**验证方式**：
- 监控 `/sys/class/net/wfb0/tx_queue_len`，确认实际队列深度
- 模拟拥塞：暂停发射端，观察 TUN 队列快速填满（应在 100 包左右）
- 检查 UFTP 日志，应频繁收到 `ENOBUFS` 错误

**应在哪个阶段解决**：Phase 3 - 客户端与服务器底层网卡架构整合

---

## 陷阱 6: 控制帧 FEC 编码导致延迟积压

### 描述
如果对 Token 控制帧也进行 FEC 前向纠错编码，需要等待积压多个数据包组成 FEC 组块，导致控制帧发射延迟不可控。在实时性要求极高的调度系统中，这种延迟是无法接受的。

### 后果
- **Token 发射不可预测**，影响调度精度
- **FEC 组块积压**，导致控制面阻塞
- **节点授权窗口重叠**，可能引发碰撞

### 预警信号
- Token 发射时间明显晚于预期
- 控制面线程阻塞，无法及时处理新 Token
- 日志显示 FEC 编码耗时过长（> 1ms）

### 防范策略
**控制帧跳过 FEC，直接发射**：在 Radiotap Header 后直接携带 TokenFrame。

**实现方式**：
```cpp
// 数据面：正常 FEC 编码
if (is_data_frame(pkt)) {
    FecPacket fec_pkt = encode_fec(pkt);  // FEC 编码
    pcap_inject(fec_pkt);
}
// 控制面：跳过 FEC
else if (is_control_frame(token)) {
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(token, buffer);
    // 直接拼接 Radiotap 模板 + TokenFrame，无 FEC
    pcap_inject_direct(buffer);
}
```

**Radiotap 模板分流**：
```cpp
// 初始化时生成两份 Radiotap 模板
uint8_t radiotap_template_data[MCS_0_LEN];
uint8_t radiotap_template_high_mcs[HIGH_MCS_LEN];

generate_radiotap_template(radiotap_template_data, MCS_0, 20MHz);
generate_radiotap_template(radiotap_template_high_mcs, current_mcs, current_bw);

// 发射时根据帧类型选择模板
if (is_control_frame) {
    // 使用最低 MCS 模板（MCS 0）
    inject_with_radiotap(token_frame, radiotap_template_data);
} else {
    // 使用高阶 MCS 模板
    inject_with_radiotap(fec_pkt, radiotap_template_high_mcs);
}
```

**验证方式**：
- Wireshark 抓包，确认 Token 帧没有 FEC 头部
- 测量 Token 发射延迟，应 < 1ms
- 对比数据帧和控制帧的 Radiotap MCS 字段

**应在哪个阶段解决**：Phase 1 - MAC 控制帧结构及序列化

---

## 陷阱 7: 固定周期探询风暴

### 描述
采用固定周期集中探询所有节点（如每 50ms 轮询一遍 10 个节点），会导致：
- **频谱占用率极高**：50ms × 10 = 500ms 持续占用，占用率 100%
- **浪费电池**：睡眠节点也被频繁唤醒，无法进入深度睡眠
- **碰撞风险**：虽然中心调度，但高密集占用仍可能引发干扰

### 后果
- **频谱污染严重**，干扰其他无线设备
- **客户端耗电快**，影响续航
- **网络吞吐量下降**，高开销导致有效带宽低

### 预警信号
- 频谱分析仪显示持续高占用（> 80%）
- 客户端功耗测试显示电流持续高
- 网络吞吐量远低于物理速率理论值

### 防范策略
**长间隔空闲巡逻 + 混合交织调度**：
- **空闲巡逻**：全员深睡时，每次仅探询 1 个节点，间隔 100ms
- **混合交织**：有活跃节点时，每 N 次活跃服务穿插 1 次睡眠探询

**实现方式**：
```cpp
// 场景 1: 空闲巡逻模式
if (aq.empty()) {
    // 每次从 SQ 抽 1 个节点
    uint8_t node = sq.front(); sq.pop();
    send_token(node, 15); // 15ms 微探询

    // 强制休眠 100ms
    sleep(100);

    // 无响应则放回 SQ 队尾
    if (!got_response) {
        sq.push(node);
    }
}

// 场景 2: 混合交织模式
else {
    for (int i = 0; i < N; i++) {  // N = 3-5
        uint8_t node = aq.front(); aq.pop();
        send_token(node, dynamic_window);
        aq.push(node); // 轮询
        guard_interval(5); // 5ms 保护间隔
    }

    // 穿插 1 次 SQ 探询
    uint8_t node = sq.front(); sq.pop();
    send_token(node, 15);
    sq.push(node);
}
```

**验证方式**：
- 频谱占用测量：全员深睡时应 < 15%
- 客户端功耗测试：应有明显降低
- 活跃节点吞吐量：探询开销占比应 < 2%

**应在哪个阶段解决**：Phase 1 - 服务端三态调度器逻辑（涉及调度策略）

---

## 陷阱 8: Watchdog 未保护导致丢包误判为断流

### 描述
如果节点在高负载时（状态 III）遇到单次 Token 丢失（无线干扰），系统立即判定为"极速枯竭"并降级为 MIN_WINDOW，导致突发数据无法快速传输，大幅降低吞吐量。

### 后果
- **吞吐量骤降**：从 300ms 窗口降到 50ms
- **状态误判**：实际上仍有大量数据积压
- **恢复周期长**：需要 2-3 个周期才能重新扩窗

### 预警信号
- 单次丢包后窗口立即降至最小值
- 日志显示节点频繁在状态 III 和状态 I 间跃迁
- 实际队列深度很大但窗口很小

### 防范策略
**Watchdog 动量拦截**：连续 3 次静默才判定为断流，单次静默保持原窗口。

**实现方式**：
```cpp
struct NodeState {
    uint8_t silence_count = 0;
    bool last_was_high_load = false;  // 标记上一轮状态
    uint16_t current_window = MIN_WINDOW;
};

uint16_t calculate_next_window(uint8_t node_id, uint16_t actual_used, uint16_t allocated) {
    auto& state = nodes[node_id];

    // 状态 I: 枯竭与睡眠（Zero-Payload Drop）
    if (actual_used < 5) {
        state.silence_count++;

        // Watchdog 保护：上一轮高负载，此次突然静默，大概率丢包
        if (state.last_was_high_load) {
            printf("Watchdog: 丢包误判拦截，保持窗口 %d ms\n", allocated);
            state.last_was_high_load = false;
            return allocated; // 保持原窗口，不自增沉默计数
        }

        if (state.silence_count >= 3) {
            printf("连续 3 次静默，降级为 MIN_WINDOW\n");
            state.current_window = MIN_WINDOW;
            state.silence_count = 0;
        }
        return state.current_window;
    }

    state.silence_count = 0;
    state.last_was_high_load = true;

    // 状态 III: 高负载贪突发（Additive Increase）
    if (actual_used >= allocated) {
        uint16_t target = allocated + STEP_UP;
        state.current_window = std::min(target, MAX_WINDOW);
    }
    // 状态 II: 需量拟合（Demand-Fitting）
    else {
        uint16_t target = actual_used + ELASTIC_MARGIN;
        state.current_window = std::max(target, MIN_WINDOW);
    }

    return state.current_window;
}
```

**验证方式**：
- 模拟丢包：人为丢失一次 Token，观察窗口是否保持不变
- 日志统计：记录状态跃迁，确认 Watchdog 拦截次数
- 吞吐量测试：对比有无 Watchdog 保护的吞吐量差异

**应在哪个阶段解决**：Phase 1 - 服务端三态调度器逻辑

---

## 陷阱 9: MAX_WINDOW 计算不当导致 UFTP 超时

### 描述
如果 MAX_WINDOW 设置过大（如 500ms @ 10 节点），当节点在队列尾部时，需要等待 `(N-1) × MAX_WINDOW = 4500ms` 才能获得授权。若 UFTP 的断连超时（GRTT）设为 3000ms，必然触发超时假死。

### 后果
- **UFTP 超时崩溃**，连接断开
- **数据传输中断**，需要重新开始
- **用户体验极差**，频繁断连重连

### 预警信号
- UFTP 日志频繁出现 "Timeout waiting for ACK"
- 服务器轮询到节点时，连接已断开
- Wireshark 抓包显示 UFTP FIN/RST 包

### 防范策略
**MAX_WINDOW 严苛小于 UFTP GRTT**：必须满足 `(N-1) × MAX_WINDOW < GRTT`。

**实现方式**：
```cpp
// 10 节点场景：UFTP GRTT 通常设为 5000ms
// 计算：9 × 300ms = 2700ms < 5000ms ✓ 安全

const uint16_t MAX_WINDOW_10_NODES = 300; // 10 节点：300ms

// 5 节点场景：UFTP GRTT 可设为 5000ms
// 计算：4 × 600ms = 2400ms < 5000ms ✓ 安全

const uint16_t MAX_WINDOW_5_NODES = 600;  // 5 节点：600ms

// 运行时根据在线节点数动态选择
uint16_t get_max_window(size_t num_nodes) {
    if (num_nodes <= 5) return 600;
    if (num_nodes <= 10) return 300;
    return 150; // 更多节点需进一步减小
}

// 初始化时验证
void validate_max_window(uint16_t max_window, size_t num_nodes, uint32_t uftp_grtt) {
    uint32_t max_wait = (num_nodes - 1) * max_window;
    if (max_wait >= uftp_grtt) {
        printf("FATAL: MAX_WINDOW %d ms 过大！最大等待 %d ms >= GRTT %d ms\n",
               max_window, max_wait, uftp_grtt);
        exit(1);
    }
}
```

**验证方式**：
- 运行时计算：打印 `(N-1) × MAX_WINDOW` 和 UFTP GRTT
- 压力测试：10 节点并发，队列尾部节点是否超时
- 调整 GRTT：观察 MAX_WINDOW 变化对断连率的影响

**应在哪个阶段解决**：Phase 1 - 服务端三态调度器逻辑

---

## 陷阱 10: 节点切换无保护间隔导致空中撞车

### 描述
在给不同节点发放 Token 时，如果没有插入保护间隔，上一节点的硬件 TX Queue 中可能还有残余射频包未发射完毕。新节点的 Token 立即发射，导致两包在空中时间重叠，引发物理层碰撞。

### 后果
- **空中碰撞率 > 0**，违背设计初衷
- **数据包丢失**，需要重传
- **吞吐量下降**，碰撞浪费带宽

### 预警信号
- Wireshark 抓包显示控制帧序列号乱序或重复
- 丢包率统计 > 0%
- 频谱分析仪显示瞬时功率重叠

### 防范策略
**5ms 保护间隔（Guard Interval）**：节点切换前强制静默 5ms。

**实现方式**：
```cpp
void send_token_with_guard(uint8_t node_id, uint16_t duration_ms) {
    // 发送 Token
    TokenFrame token{0x02, node_id, duration_ms, next_seq_num++};
    send_token_triplicate(token); // 三连发

    // 强制静默 5ms，确保上一节点硬件队列排空
    precise_sleep_ms(5); // 使用 timerfd + 空转自旋
}

// 调度器主循环
for (int i = 0; i < N; i++) {
    uint8_t node = aq.front(); aq.pop();
    send_token_with_guard(node, dynamic_window);
    aq.push(node); // 轮询
}
```

**高精度休眠实现**：
```cpp
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

void precise_sleep_ms(uint32_t ms) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // 计算目标时间
    uint64_t target_ns = ts.tv_sec * 1'000'000'000LL + ts.tv_nsec + ms * 1'000'000LL;

    // 短时间（< 100μs）空转自旋，避免上下文切换
    if (ms < 1) {
        uint64_t start_ns = ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
        uint64_t elapsed_ns = 0;
        while (elapsed_ns < target_ns) {
            clock_gettime(CLOCK_MONOTONIC, &ts);
            elapsed_ns = ts.tv_sec * 1'000'000'000LL + ts.tv_nsec - start_ns;
        }
        return;
    }

    // 长时间使用 timerfd
    int timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    struct itimerspec its;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = ms * 1'000'000LL;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    timerfd_settime(timerfd, 0, &its, NULL);

    // 超前极短时间空转自旋（< 100μs）
    uint64_t spin_ns = 100'000; // 100μs
    uint64_t spin_target_ns = target_ns - spin_ns;
    uint64_t start_ns = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start_ns = ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
    uint64_t elapsed_ns = 0;
    while (elapsed_ns < spin_target_ns) {
        clock_gettime(CLOCK_MONOTONIC, &ts);
        elapsed_ns = ts.tv_sec * 1'000'000'000LL + ts.tv_nsec - start_ns;
    }

    // 等待 timerfd
    uint64_t expirations;
    read(timerfd, &expirations, sizeof(expirations));

    close(timerfd);
}
```

**验证方式**：
- Wireshark 抓包：相邻控制帧时间间隔应 ≥ 5ms
- 碰撞统计：应为 0%
- 功率分析仪：瞬时功率不应重叠

**应在哪个阶段解决**：Phase 1 - MAC 控制帧结构及序列化（涉及发射时序）

---

## 总结：陷阱优先级矩阵

| 陷阱编号 | 陷阱名称 | 严重性 | 发现难度 | 应对阶段 |
|---------|----------|---------|-----------|---------|
| 1 | 双进程通信环路数据静默丢失 | P0 | 中 | Phase 3 |
| 2 | 水位线硬编码导致 Bufferbloat | P0 | 中 | Phase 2 |
| 3 | 实时调度未提权导致毛刺失控 | P0 | 低 | Phase 1 |
| 4 | usleep() 导致灾难性轮询延迟 | P0 | 中 | Phase 1 |
| 5 | TUN 队列设置过大导致反压失效 | P0 | 低 | Phase 3 |
| 6 | 控制帧 FEC 编码导致延迟积压 | P1 | 中 | Phase 1 |
| 7 | 固定周期探询风暴 | P1 | 低 | Phase 1 |
| 8 | Watchdog 未保护导致丢包误判 | P1 | 高 | Phase 1 |
| 9 | MAX_WINDOW 计算不当导致 UFTP 超时 | P0 | 中 | Phase 1 |
| 10 | 节点切换无保护间隔导致空中撞车 | P0 | 中 | Phase 1 |

## 陷阱检测检查清单

在代码审查和测试阶段，逐项检查以下问题：

- [ ] 代码中是否存在独立进程通过 UDP 通信？
- [ ] 水位线计算是否使用 MCS 动态映射表？
- [ ] 关键线程是否提权至 SCHED_FIFO？
- [ ] 是否使用 `condition_variable` 替代 `usleep()`？
- [ ] TUN 设备初始化时是否设置 `txqueuelen 100`？
- [ ] 控制帧是否跳过 FEC 编码？
- [ ] 调度器是否实现了长间隔空闲巡逻？
- [ ] 三态状态机是否包含 Watchdog 保护？
- [ ] MAX_WINDOW 是否根据在线节点数动态调整？
- [ ] 节点切换时是否插入 5ms 保护间隔？

---

*Research generated: 2026-04-22*
