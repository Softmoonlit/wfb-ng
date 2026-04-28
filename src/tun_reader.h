#ifndef TUN_READER_H
#define TUN_READER_H

#include "threads.h"
#include "config.h"
#include <atomic>
#include <string>
#include <cstdint>

/**
 * TUN 读取线程配置
 *
 * 用于配置 TUN 设备创建和读取线程行为。
 * 遵循 CLAUDE.md 要求：txqueuelen 必须设置为 100（极小水池反压）
 */
struct TunConfig {
    std::string tun_name;       // TUN 设备名（如 "wfb0"）
    int txqueuelen;             // 队列长度（默认 100，极小水池反压）
    FECConfig fec;              // FEC 参数
    int mcs;                    // MCS 调制方案（0-8）
    int bandwidth;              // 频宽（MHz，20 或 40）
    uint32_t watermark_window_ms;  // 水位线计算窗口（默认 50ms）
};

/**
 * TUN 读取线程错误码
 *
 * 用于标识 TUN 读取线程执行过程中的错误类型。
 * 所有系统调用错误都必须有对应的错误码和恢复逻辑。
 */
enum class TunError {
    OK = 0,                     // 成功
    TUN_OPEN_FAILED,            // 打开 TUN 设备失败
    TUN_IOCTL_FAILED,           // TUN 设备配置失败
    TXQUEUELEN_SET_FAILED,      // 设置队列长度失败
    FEC_INIT_FAILED,            // FEC 编码器初始化失败
    THREAD_SHUTDOWN,            // 线程正常关闭
    READ_ERROR,                 // 读取 TUN 设备失败
    UNKNOWN_ERROR               // 未知错误
};

/**
 * TUN 读取线程统计信息
 *
 * 使用原子操作确保线程安全，无需额外锁保护。
 * 用于监控 TUN 读取线程的性能和健康状况。
 */
struct TunStats {
    std::atomic<uint64_t> packets_read{0};          // 读取的数据包数
    std::atomic<uint64_t> packets_dropped{0};       // 丢弃的数据包数（反压）
    std::atomic<uint64_t> fec_blocks_encoded{0};    // FEC 编码的块数
    std::atomic<uint64_t> backpressure_events{0};   // 反压事件次数
    std::atomic<uint64_t> read_errors{0};           // 读取错误次数
};

/**
 * TUN 读取线程主函数
 *
 * 负责从 TUN 设备读取数据包，进行 FEC 编码，并放入共享队列。
 * 遵循 RT-04：TUN 读取线程保留普通优先级（CFS），避免 FEC 编码 CPU 密集型导致系统死机。
 *
 * @param shared_state 线程共享状态
 * @param config TUN 配置
 * @param shutdown 关闭信号（原子布尔值）
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
 * 创建并配置 TUN 网络设备，设置为非阻塞模式。
 * 所有系统调用错误都有处理和恢复逻辑。
 *
 * @param name TUN 设备名（如 "wfb0"）
 * @param error_buf 错误缓冲区（可选）
 * @return 文件描述符（失败返回 -1）
 */
int create_tun_device(const std::string& name, char* error_buf);

/**
 * 设置 TUN 队列长度
 *
 * 使用 ip 命令设置 TUN 设备的队列长度。
 * 遵循 CLAUDE.md 要求：必须设置 txqueuelen 100 实现极小水池反压。
 *
 * @param name TUN 设备名
 * @param qlen 队列长度（必须为 100）
 * @return 成功返回 true
 */
bool set_tun_txqueuelen(const std::string& name, int qlen);

#endif  // TUN_READER_H