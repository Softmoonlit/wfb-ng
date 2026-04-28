#ifndef WFB_RX_DEMUX_H
#define WFB_RX_DEMUX_H

#include "threads.h"
#include "config.h"
#include <atomic>
#include <cstdint>

// pcap 前向声明
struct pcap;
typedef struct pcap pcap_t;

// RX 线程配置
struct RxConfig {
    const char* interface;       // WiFi 接口名
    int channel;                 // 信道号
    int pcap_timeout_ms;         // pcap 超时（默认 10ms）
    uint8_t local_node_id;       // 本地节点 ID（客户端用）
    bool is_server;              // 是否为服务端
};

// RX 线程错误码
enum class RxError {
    OK = 0,
    PCAP_OPEN_FAILED,
    PCAP_SET_FILTER_FAILED,
    PCAP_ACTIVATE_FAILED,
    INTERFACE_NOT_FOUND,
    THREAD_SHUTDOWN,
    UNKNOWN_ERROR
};

// RX 线程统计
struct RxStats {
    std::atomic<uint64_t> packets_received{0};
    std::atomic<uint64_t> tokens_parsed{0};
    std::atomic<uint64_t> parse_errors{0};
    std::atomic<uint64_t> pcap_errors{0};
};

/**
 * RX 解复用线程主函数
 *
 * @param shared_state 线程共享状态
 * @param config RX 配置
 * @param shutdown 关闭信号
 * @param stats 统计信息（可选）
 * @return 错误码
 */
RxError rx_demux_main_loop(
    ThreadSharedState* shared_state,
    const RxConfig& config,
    std::atomic<bool>* shutdown,
    RxStats* stats = nullptr
);

/**
 * 初始化 pcap 句柄
 *
 * @param interface 网络接口名
 * @param channel 信道号
 * @param error_buf 错误缓冲区
 * @return pcap 句柄（需调用者关闭）或 nullptr
 */
pcap_t* init_pcap_handle(const char* interface, int channel, char* error_buf);

#endif  // WFB_RX_DEMUX_H