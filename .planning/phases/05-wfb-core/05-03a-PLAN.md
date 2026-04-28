---
phase: 05
plan: 03a
type: execute
wave: 3
depends_on:
  - 05-00
  - 05-01
files_modified:
  - src/tun_reader.h
  - src/tun_reader.cpp
autonomous: true
requirements:
  - PROC-03
  - PROC-08
  - RT-04

must_haves:
  truths:
    - "TUN 设备正确创建并配置"
    - "txqueuelen 设置为 100（极小水池反压）"
    - "所有系统调用错误都有处理和恢复逻辑"
  artifacts:
    - path: "src/tun_reader.h"
      provides: "TUN 读取线程接口"
      contains: "tun_reader_main_loop"
    - path: "src/tun_reader.cpp"
      provides: "TUN 设备创建和配置实现"
      min_lines: 150
  key_links:
    - from: "src/tun_reader.cpp"
      to: "/dev/net/tun"
      via: "TUN 设备创建"
      pattern: "open.*tun"
    - from: "src/tun_reader.cpp"
      to: "ip link"
      via: "txqueuelen 设置"
      pattern: "txqueuelen"
---

# Plan 05-03a: TUN 设备创建与配置

<objective>
创建 TUN 读取线程基础设施，实现 TUN 设备创建、txqueuelen 配置和错误处理。

**拆分自 Plan 05-03**（任务过多，按 checker 建议拆分）

Purpose: 建立稳健的 TUN 设备创建和配置流程，实现极小水池反压机制
Output: 生产就绪的 TUN 设备创建接口，包含完整错误处理
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
From src/threads.h:
```cpp
struct ThreadSharedState {
    std::mutex mtx;
    std::condition_variable cv_send;
    bool can_send GUARDED_BY(mtx) = false;
    PacketQueue packet_queue;
    std::atomic<bool> shutdown{false};
};
```

From src/config.h (Plan 05-00):
```cpp
struct FECConfig {
    int n = 12;
    int k = 8;
    bool is_valid() const;
};
```
</interfaces>
</context>

<tasks>

<task type="auto" tdd="true">
  <name>Task 1: 创建 TUN 读取线程头文件和接口</name>
  <files>src/tun_reader.h</files>
  <behavior>
    - Test 1: tun_reader_main_loop 函数签名验证
    - Test 2: TunConfig 结构体验证
    - Test 3: 错误码定义验证
  </behavior>
  <action>
定义 TUN 读取线程接口，包含错误处理和配置

```cpp
#ifndef TUN_READER_H
#define TUN_READER_H

#include "threads.h"
#include "config.h"
#include <atomic>
#include <string>
#include <cstdint>

// TUN 读取线程配置
struct TunConfig {
    std::string tun_name;       // TUN 设备名
    int txqueuelen;             // 队列长度（默认 100）
    FECConfig fec;              // FEC 参数
    int mcs;                    // MCS 调制方案
    int bandwidth;              // 频宽（MHz）
    uint32_t watermark_window_ms;  // 水位线计算窗口（默认 50ms）
};

// TUN 读取线程错误码
enum class TunError {
    OK = 0,
    TUN_OPEN_FAILED,
    TUN_IOCTL_FAILED,
    TXQUEUELEN_SET_FAILED,
    FEC_INIT_FAILED,
    THREAD_SHUTDOWN,
    READ_ERROR,
    UNKNOWN_ERROR
};

// TUN 读取线程统计
struct TunStats {
    std::atomic<uint64_t> packets_read{0};
    std::atomic<uint64_t> packets_dropped{0};
    std::atomic<uint64_t> fec_blocks_encoded{0};
    std::atomic<uint64_t> backpressure_events{0};
    std::atomic<uint64_t> read_errors{0};
};

/**
 * TUN 读取线程主函数
 *
 * @param shared_state 线程共享状态
 * @param config TUN 配置
 * @param shutdown 关闭信号
 * @param stats 统计信息（可选）
 * @return 错误码
 */
TunError tun_reader_main_loop(
    ThreadSharedState* shared_state,
    const TunConfig& config,
    std::atomic<bool>* shutdown,
    TunStats* stats = nullptr
);

/**
 * 创建 TUN 设备
 *
 * @param name TUN 设备名
 * @param error_buf 错误缓冲区
 * @return 文件描述符（失败返回 -1）
 */
int create_tun_device(const std::string& name, char* error_buf);

/**
 * 设置 TUN 队列长度
 *
 * @param name TUN 设备名
 * @param qlen 队列长度
 * @return 成功返回 true
 */
bool set_tun_txqueuelen(const std::string& name, int qlen);

#endif  // TUN_READER_H
```
  </action>
  <verify>
    <automated>grep -E "tun_reader_main_loop|TunConfig|TunError" src/tun_reader.h && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>
    - TUN 读取线程接口定义完整
    - 错误码定义清晰
    - 配置结构体完整
    - FEC 参数可配置
  </done>
</task>

<task type="auto" tdd="true">
  <name>Task 2: 实现 TUN 设备创建和配置</name>
  <files>src/tun_reader.cpp</files>
  <behavior>
    - Test 1: TUN 设备创建成功
    - Test 2: TUN 设备名正确设置
    - Test 3: txqueuelen 设置成功
    - Test 4: 无效接口名返回错误
  </behavior>
  <action>
实现 TUN 设备创建和配置，包含完整错误处理

```cpp
#include "tun_reader.h"
#include "watermark.h"
#include "error_handler.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

int create_tun_device(const std::string& name, char* error_buf) {
    // 参数验证
    if (name.empty() || name.length() >= IFNAMSIZ) {
        if (error_buf) {
            snprintf(error_buf, 256, "TUN 设备名无效: %s", name.c_str());
        }
        return -1;
    }

    // 打开 TUN 设备
    int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        if (error_buf) {
            snprintf(error_buf, 256, "无法打开 /dev/net/tun: %s",
                     strerror(errno));
        }
        return -1;
    }

    // 配置 TUN 设备
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);

    // 创建 TUN 接口
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        if (error_buf) {
            snprintf(error_buf, 256, "TUNSETIFF 失败: %s", strerror(errno));
        }
        close(fd);
        return -1;
    }

    std::cout << "TUN 设备创建成功: " << name << " (fd=" << fd << ")" << std::endl;
    return fd;
}

bool set_tun_txqueuelen(const std::string& name, int qlen) {
    if (name.empty() || qlen <= 0) {
        std::cerr << "参数无效: name=" << name << ", qlen=" << qlen << std::endl;
        return false;
    }

    // 使用 ip 命令设置队列长度
    std::string cmd = "ip link set dev " + name + " txqueuelen " +
                      std::to_string(qlen) + " 2>/dev/null";
    int ret = system(cmd.c_str());

    if (ret != 0) {
        std::cerr << "警告: 设置 txqueuelen 失败，将使用默认值" << std::endl;
        return false;
    }

    std::cout << "TUN 队列长度设置为: " << qlen << std::endl;
    return true;
}
```
  </action>
  <verify>
    <automated>grep -E "create_tun_device|set_tun_txqueuelen" src/tun_reader.cpp && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>
    - TUN 设备创建完整实现
    - 所有错误情况都有处理
    - txqueuelen 设置实现
    - 非阻塞模式设置
  </done>
</task>

<task type="auto">
  <name>Task 3: 更新 wfb_core.cpp 集成 TUN 读取线程</name>
  <files>src/wfb_core.cpp</files>
  <action>
在主程序中启动 TUN 读取线程骨架

```cpp
#include "tun_reader.h"

// 在 run_server() 和 run_client() 中

// TUN 配置
TunConfig tun_config = {
    .tun_name = config.tun_name,
    .txqueuelen = 100,  // 极小水池
    .fec = config.fec,
    .mcs = config.mcs,
    .bandwidth = config.bandwidth,
    .watermark_window_ms = 50
};

// 启动 TUN 读取线程（普通优先级，不设置实时调度，Per RT-04）
TunStats tun_stats;
std::thread tun_thread([&]() {
    // RT-04: TUN 读取线程保留普通优先级（CFS），避免 FEC 编码 CPU 密集型导致系统死机
    // 不调用 set_thread_realtime_priority()
    TunError err = tun_reader_main_loop(&shared_state, tun_config,
                                        &g_shutdown, &tun_stats);

    if (err != TunError::OK) {
        std::cerr << "TUN 读取线程异常退出: " << static_cast<int>(err) << std::endl;
    }
});

// ... 等待所有线程 ...

tun_thread.join();

// 输出统计信息
std::cout << "TUN 统计: 读取=" << tun_stats.packets_read.load()
          << ", 丢弃=" << tun_stats.packets_dropped.load()
          << ", FEC编码=" << tun_stats.fec_blocks_encoded.load()
          << ", 反压事件=" << tun_stats.backpressure_events.load() << std::endl;
```
  </action>
  <verify>
    <automated>grep -E "tun_reader_main_loop|TunConfig" src/wfb_core.cpp && echo "PASS" || echo "FAIL"</automated>
  </verify>
  <done>
    - TUN 读取线程正确启动
    - FEC 参数从配置读取
    - txqueuelen 设置为 100
    - RT-04: 不设置实时优先级
  </done>
</task>

</tasks>

<threat_model>
## Trust Boundaries

| Boundary | Description |
|----------|-------------|
| TUN 设备 → 读取线程 | 内核数据，可信 |

## STRIDE Threat Register

| Threat ID | Category | Component | Disposition | Mitigation Plan |
|-----------|----------|-----------|-------------|-----------------|
| T-05-08 | DoS | TUN 读取 | mitigate | 水位线反压，防止内存耗尽 |
</threat_model>

<verification>
- TUN 读取线程编译通过
- TUN 设备创建正确
- txqueuelen 设置为 100
</verification>

<success_criteria>
1. TUN 设备正确创建并配置
2. txqueuelen 设置为 100
3. 所有系统调用错误都有处理
4. RT-04: TUN 读取线程不设置实时优先级
</success_criteria>

<output>
After completion, create `.planning/phases/05-wfb-core/05-03a-SUMMARY.md`
</output>
