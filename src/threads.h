#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "packet_queue.h"

// 控制面线程默认使用最高实时优先级。
static const int kControlThreadPriority = 99;
// 发射线程使用次高实时优先级。
static const int kTxThreadPriority = 90;

// 动态水位线输入来自 MCS 映射后的物理速率。
static const uint64_t kDefaultPhyRateBps = 6000000ULL;
// 5ms 保护间隔是阶段的固定保护窗。
static const uint16_t kGuardIntervalMs = 5;

// 统一的实时调度提权入口。
void set_thread_realtime_priority(int priority);

// 单进程入口里的共享状态，保持粒度最小。
struct ThreadSharedState {
    ThreadSharedState() : can_send(false), token_expire_time_ms(0) {}

    bool can_send;
    uint64_t token_expire_time_ms;
    std::condition_variable cv_send;
    std::mutex mtx;
    ThreadSafeQueue<uint8_t> packet_queue{1};
};

// 控制面主循环：负责唤醒和状态更新。
void control_main_loop(ThreadSharedState* shared_state);
// 发射主循环：负责检查阀门并进入空口发射。
void tx_main_loop(ThreadSharedState* shared_state);
// TUN 读取主循环：保持普通优先级，不做提权。
void tun_reader_main_loop(ThreadSharedState* shared_state);
