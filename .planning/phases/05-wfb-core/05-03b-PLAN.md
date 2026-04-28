---
phase: 05
plan: 03b
type: execute
wave: 4
depends_on:
  - 05-03a
files_modified:
  - src/tun_reader.cpp
  - src/resource_guard.h
autonomous: true
requirements:
  - BUF-05

must_haves:
  truths:
    - "FEC 编码器正确初始化和管理"
    - "队列达到水位线时停止读取，制造反压"
    - "TUN 文件描述符和 FEC 编码器使用 RAII 管理"
  artifacts:
    - path: "src/tun_reader.cpp"
      provides: "TUN 读取线程完整实现"
      min_lines: 250
    - path: "src/resource_guard.h"
      provides: "RAII 资源管理器"
      contains: "ZfexCodeHandle|FdHandle"
  key_links:
    - from: "src/tun_reader.cpp"
      to: "src/resource_guard.h"
      via: "FEC 编码器和 TUN fd 管理"
      pattern: "ZfexCodeHandle|FdHandle"
    - from: "src/tun_reader.cpp"
      to: "src/watermark.h"
      via: "动态水位线计算"
      pattern: "calculate_dynamic_limit"
---

# Plan 05-03b: FEC 编码与反压机制

<objective>
实现 TUN 读取线程的 FEC 编码、动态水位线反压机制和 RAII 资源管理。

**拆分自 Plan 05-03**（任务过多，按 checker 建议拆分）

Purpose: 建立完整的 FEC 编码流程和反压机制，确保资源安全释放
Output: 生产就绪的 TUN 读取线程主循环，包含 RAII 资源管理
</objective>

<execution_context>
@/home/kilome/wfb-ng/.claude/get-shit-done/workflows/execute-plan.md
@/home/kilome/wfb-ng/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/PROJECT.md
@.planning/ROADMAP.md
@.planning/STATE.md
@.planning/phases/05-wfb-core/05-CONTEXT.md
@.planning/phases/05-wfb-core/05-RESEARCH.md

<interfaces>
From src/watermark.h:
```cpp
uint32_t calculate_dynamic_limit(uint32_t duration_ms,
                                  uint64_t phy_rate_bps,
                                  uint16_t mtu);
uint64_t get_phy_rate_bps(int mcs, int bandwidth_mhz);
```

From src/zfex.h:
```cpp
typedef struct zfex_code zfex_code;
zfex_code* zfex_code_create(zfex_status* status, int n, int k);
void zfex_code_destroy(zfex_code* code);
void zfex_code_encode(zfex_code* code, const uint8_t** inputs, uint8_t** outputs);
```

From src/tun_reader.h (Plan 05-03a):
```cpp
TunError tun_reader_main_loop(
    ThreadSharedState* shared_state,
    const TunConfig& config,
    std::atomic<bool>* shutdown,
    TunStats* stats
);
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: 更新 resource_guard.h 添加 FEC 和 FD 句柄</name>
  <files>src/resource_guard.h</files>
  <action>
添加 FEC 编码器和文件描述符的智能指针封装

```cpp
// 在 resource_guard.h 中添加（如果尚未存在）

// FEC 编码器智能指针
struct ZfexCodeDeleter {
    void operator()(zfex_code* ptr) const {
        if (ptr) zfex_code_destroy(ptr);
    }
};
using ZfexCodeHandle = std::unique_ptr<zfex_code, ZfexCodeDeleter>;

inline ZfexCodeHandle make_zfex_handle(zfex_code* raw) {
    if (!raw) return nullptr;
    return ZfexCodeHandle(raw);
}

// 文件描述符智能指针
struct FdDeleter {
    void operator()(int* fd) const {
        if (fd && *fd >= 0) {
            close(*fd);
            delete fd;
        }
    }
};
using FdHandle = std::unique_ptr<int, FdDeleter>;

inline FdHandle make_fd_handle(int raw_fd) {
    if (raw_fd < 0) return nullptr;
    return FdHandle(new int(raw_fd));
}
```
  </action>
  <verify>
    <automated>grep -E "ZfexCodeHandle|FdHandle|make_zfex_handle|make_fd_handle" src/resource_guard.h && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>
    - ZfexCodeHandle 智能指针定义
    - FdHandle 智能指针定义
    - 辅助函数实现
  </done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: 实现 TUN 读取线程主循环</name>
  <files>src/tun_reader.cpp</files>
  <behavior>
    - Test 1: TUN 数据正确读取
    - Test 2: 队列满时制造反压
    - Test 3: FEC 编码正确执行
    - Test 4: shutdown 信号触发后快速退出
    - Test 5: FEC 编码器资源正确释放
  </behavior>
  <action>
实现完整的 TUN 读取线程主循环，包含 FEC 编码和反压机制

```cpp
namespace {

// 读取超时（毫秒）
constexpr int kReadTimeoutMs = 10;

// FEC 块最大长度
constexpr size_t kMaxBlockLen = 1450;

}  // namespace

TunError tun_reader_main_loop(
    ThreadSharedState* shared_state,
    const TunConfig& config,
    std::atomic<bool>* shutdown,
    TunStats* stats
) {
    // 参数验证
    if (!shared_state || !shutdown) {
        return TunError::UNKNOWN_ERROR;
    }

    char error_buf[256] = {0};

    // 1. 创建 TUN 设备
    int tun_fd = create_tun_device(config.tun_name, error_buf);
    if (tun_fd < 0) {
        std::cerr << "TUN 设备创建失败: " << error_buf << std::endl;
        return TunError::TUN_OPEN_FAILED;
    }

    // 使用 RAII 管理 TUN fd
    auto tun_fd_guard = make_fd_handle(tun_fd);

    // 2. 设置极小队列长度（反压关键）
    if (!set_tun_txqueuelen(config.tun_name, config.txqueuelen)) {
        std::cerr << "警告: txqueuelen 设置失败，继续使用默认值" << std::endl;
    }

    // 3. 初始化 FEC 编码器
    zfex_status fec_status;
    zfex_code* raw_fec = zfex_code_create(&fec_status,
                                          config.fec.n,
                                          config.fec.k);
    if (!raw_fec) {
        std::cerr << "FEC 编码器创建失败: " << fec_status.error << std::endl;
        return TunError::FEC_INIT_FAILED;
    }

    // 使用 RAII 管理 FEC 编码器
    auto fec_code = make_zfex_handle(raw_fec);

    // 4. 计算动态水位线
    uint64_t phy_rate = get_phy_rate_bps(config.mcs, config.bandwidth);
    uint32_t dynamic_limit = calculate_dynamic_limit(
        config.watermark_window_ms, phy_rate, kMaxBlockLen);

    std::cout << "TUN 读取线程启动，水位线=" << dynamic_limit
              << ", FEC(N=" << config.fec.n << ", K=" << config.fec.k << ")"
              << std::endl;

    // 5. FEC 块缓冲区
    std::vector<std::vector<uint8_t>> fec_blocks(config.fec.n);
    for (auto& block : fec_blocks) {
        block.resize(kMaxBlockLen);
    }

    std::vector<const uint8_t*> fec_inputs(config.fec.k);
    std::vector<uint8_t*> fec_outputs(config.fec.n - config.fec.k);

    size_t block_count = 0;
    int consecutive_errors = 0;
    constexpr int kMaxConsecutiveErrors = 10;

    // 6. 主循环
    while (!shutdown->load()) {
        // 检查队列是否达到水位线
        if (shared_state->packet_queue.size() >= dynamic_limit) {
            // 队列满，制造反压
            if (stats) stats->backpressure_events++;
            usleep(1000);  // 1ms 退避
            continue;
        }

        // 读取 TUN 数据
        ssize_t nread = read(tun_fd, fec_blocks[block_count].data(), kMaxBlockLen);

        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下的正常情况
                usleep(kReadTimeoutMs * 1000);
                continue;
            }

            // 真正的错误
            consecutive_errors++;
            if (stats) stats->read_errors++;
            std::cerr << "TUN 读取错误: " << strerror(errno) << std::endl;

            if (consecutive_errors >= kMaxConsecutiveErrors) {
                std::cerr << "连续错误过多，退出 TUN 读取线程" << std::endl;
                return TunError::READ_ERROR;
            }

            usleep(10000);  // 10ms 错误退避
            continue;
        }

        // 重置连续错误计数
        consecutive_errors = 0;

        if (nread == 0) {
            continue;  // 空读
        }

        if (stats) stats->packets_read++;

        // 记录实际长度
        fec_blocks[block_count].resize(static_cast<size_t>(nread));
        fec_inputs[block_count] = fec_blocks[block_count].data();
        block_count++;

        // 当收集到 K 个块时进行 FEC 编码
        if (block_count >= static_cast<size_t>(config.fec.k)) {
            // 准备输出缓冲区
            for (int i = 0; i < config.fec.n - config.fec.k; i++) {
                fec_outputs[i] = fec_blocks[config.fec.k + i].data();
            }

            // FEC 编码
            zfex_code_encode(fec_code.get(),
                             fec_inputs.data(),
                             fec_outputs.data());

            if (stats) stats->fec_blocks_encoded++;

            // 将所有块入队
            for (int i = 0; i < config.fec.n; i++) {
                PacketQueue::Packet pkt;
                pkt.len = (i < config.fec.k)
                              ? fec_blocks[i].size()
                              : kMaxBlockLen;  // 冗余块固定大小
                memcpy(pkt.data, fec_blocks[i].data(), pkt.len);

                if (!shared_state->packet_queue.push(pkt)) {
                    // 队列满，丢弃
                    if (stats) stats->packets_dropped++;
                    break;
                }
            }

            // 重置块计数
            block_count = 0;

            // 通知发射线程有新数据
            shared_state->cv_send.notify_one();
        }
    }

    std::cout << "TUN 读取线程正常退出" << std::endl;
    return TunError::OK;
}
```
  </action>
  <verify>
    <automated>make tun_reader && echo "编译通过" || echo "编译失败"</automated>
  </verify>
  <done>
    - TUN 读取线程正确实现
    - FEC 编码器使用 RAII 管理
    - 动态水位线反压机制
    - 所有错误情况都有处理
    - FEC 参数可配置
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| 读取线程 → FEC 编码 | 内部处理，可信 |
| 读取线程 → 队列 | 内部数据结构，可信 |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-05-09 | Info Disclosure | FEC 数据 | accept | 数据已加密或为公开信息 |
</threat_model>

<verification>
- TUN 读取线程编译通过
- FEC 编码器 RAII 管理正确
- 动态水位线反压机制正常
</verification>

<success_criteria>
1. FEC 编码器正确初始化和管理
2. 队列达到水位线时停止读取
3. TUN fd 和 FEC 编码器使用 RAII 管理
</success_criteria>

<output>
After completion, create `.planning/phases/05-wfb-core/05-03b-SUMMARY.md`
</output>
