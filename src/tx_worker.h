#ifndef TX_WORKER_H
#define TX_WORKER_H

#include "threads.h"
#include <pcap.h>
#include <atomic>
#include <cstdint>

// pcap 前向声明
typedef struct pcap pcap_t;

// TX 线程配置
struct TxConfig {
    int mcs;                    // MCS 调制方案
    int bandwidth;              // 频宽（MHz）
    uint32_t batch_size;        // 批量发射大小（默认 32）
    uint32_t inject_retries;    // pcap_inject 重试次数（默认 3）
    uint32_t inject_retry_delay_us;  // 重试延迟（默认 100us）
};

// TX 线程错误码
enum class TxError {
    OK = 0,
    PCAP_INJECT_FAILED,
    FRAME_BUILD_FAILED,
    THREAD_SHUTDOWN,
    TOKEN_EXPIRED
};

// TX 线程统计
struct TxStats {
    std::atomic<uint64_t> packets_sent{0};
    std::atomic<uint64_t> packets_failed{0};
    std::atomic<uint64_t> inject_retries{0};
    std::atomic<uint64_t> token_expired{0};
};

/**
 * TX 发射线程主函数
 *
 * @param shared_state 线程共享状态
 * @param pcap_handle pcap 句柄
 * @param config TX 配置
 * @param shutdown 关闭信号
 * @param stats 统计信息（可选）
 */
TxError tx_main_loop(
    ThreadSharedState* shared_state,
    pcap_t* pcap_handle,
    const TxConfig& config,
    std::atomic<bool>* shutdown,
    TxStats* stats = nullptr
);

#endif  // TX_WORKER_H