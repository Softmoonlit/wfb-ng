#ifndef SCHEDULER_WORKER_H
#define SCHEDULER_WORKER_H

#include "threads.h"
#include "config.h"
#include <atomic>
#include <pcap.h>
#include <cstdint>

// 前向声明
class ServerScheduler;

// 调度器线程配置
struct SchedulerConfig {
    uint32_t min_window_ms;     // 最小授权窗口（默认 50ms）
    uint32_t max_window_ms;     // 最大授权窗口（默认 300ms）
    uint32_t guard_interval_ms; // 保护间隔（默认 5ms）
    uint8_t node_count;         // 节点数量
};

// 调度器线程错误码
enum class SchedulerError {
    OK = 0,
    TOKEN_SEND_FAILED,
    NO_NODES_REGISTERED,
    THREAD_SHUTDOWN,
    UNKNOWN_ERROR
};

// 调度器线程统计
struct SchedulerStats {
    std::atomic<uint64_t> tokens_sent{0};
    std::atomic<uint64_t> tokens_failed{0};
    std::atomic<uint64_t> idle_patrols{0};
    std::atomic<uint64_t> node_switches{0};
};

/**
 * 调度器线程主函数（仅 Server 模式）
 *
 * @param shared_state 线程共享状态
 * @param scheduler 调度器实例
 * @param pcap_handle pcap 句柄
 * @param config 调度器配置
 * @param shutdown 关闭信号
 * @param stats 统计信息（可选）
 */
SchedulerError scheduler_main_loop(
    ThreadSharedState* shared_state,
    ServerScheduler* scheduler,
    pcap_t* pcap_handle,
    const SchedulerConfig& config,
    std::atomic<bool>* shutdown,
    SchedulerStats* stats = nullptr
);

#endif  // SCHEDULER_WORKER_H