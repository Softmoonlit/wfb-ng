#include "config.h"
#include "error_handler.h"
#include "threads.h"
#include "rx_demux.h"
#include "tx_worker.h"
#include "tun_reader.h"

#include <atomic>
#include <csignal>
#include <thread>
#include <vector>
#include <iostream>

// 全局关闭信号
static std::atomic<bool> g_shutdown{false};

// 信号处理函数
void signal_handler(int sig) {
    std::cout << "\n收到信号 " << sig << "，正在关闭..." << std::endl;
    g_shutdown.store(true);
}

// Server 模式主函数（骨架，后续计划填充）
int run_server(const Config& config);

// Client 模式主函数（骨架，后续计划填充）
int run_client(const Config& config);

int main(int argc, char** argv) {
    // 1. 解析命令行参数
    Config config;
    if (!parse_arguments(argc, argv, config)) {
        std::cerr << "参数解析失败，使用 --help 查看用法" << std::endl;
        return 1;
    }

    // 2. 设置错误处理回调
    set_error_callback([](const ErrorContext& ctx) {
        std::cerr << "[" << ctx.file << ":" << ctx.line << "] "
                  << ctx.message << std::endl;
        if (ctx.severity == ErrorSeverity::FATAL) {
            exit(1);
        }
    });

    // 3. 注册信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 4. Root 权限检查
    if (geteuid() != 0) {
        std::cerr << "警告: 必须以 Root 权限运行才能设置实时调度" << std::endl;
        // 继续，但实时调度可能失败
    }

    // 5. 打印配置信息
    if (config.verbose) {
        std::cout << "配置信息:\n"
                  << "  模式: " << (config.mode == Config::Mode::SERVER ? "服务端" : "客户端") << "\n"
                  << "  接口: " << config.interface << "\n"
                  << "  信道: " << config.channel << "\n"
                  << "  MCS: " << config.mcs << "\n"
                  << "  频宽: " << config.bandwidth << "MHz\n"
                  << "  TUN设备: " << config.tun_name << "\n"
                  << "  FEC参数: N=" << config.fec.n << ", K=" << config.fec.k << "\n";
        if (config.mode == Config::Mode::CLIENT) {
            std::cout << "  节点ID: " << static_cast<int>(config.node_id) << "\n";
        }
    }

    // 6. 根据模式执行
    int ret = 0;
    switch (config.mode) {
        case Config::Mode::SERVER:
            std::cout << "启动服务端模式..." << std::endl;
            ret = run_server(config);
            break;
        case Config::Mode::CLIENT:
            std::cout << "启动客户端模式，节点 ID: "
                      << static_cast<int>(config.node_id) << std::endl;
            ret = run_client(config);
            break;
    }

    return ret;
}

int run_server(const Config& config) {
    ThreadSharedState shared_state;

    // RX 配置
    RxConfig rx_config = {
        .interface = config.interface.c_str(),
        .channel = config.channel,
        .pcap_timeout_ms = 10,
        .local_node_id = 0,
        .is_server = true
    };

    // 启动 RX 线程
    std::thread rx_thread([&]() {
        // 设置实时优先级 99
        if (!set_thread_realtime_priority(ThreadPriority::CONTROL_PLANE, "rx_demux")) {
            std::cerr << "警告: 无法设置 RX 线程实时优先级" << std::endl;
        }

        RxStats stats;
        RxError err = rx_demux_main_loop(&shared_state, rx_config, &g_shutdown, &stats);

        if (err != RxError::OK) {
            std::cerr << "RX 线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    // TUN 配置（遵循 CLAUDE.md：txqueuelen 设置为 100，极小水池反压）
    TunConfig tun_config;
    tun_config.tun_name = config.tun_name;
    tun_config.txqueuelen = 100;  // 极小水池反压
    tun_config.fec = config.fec;
    tun_config.mcs = config.mcs;
    tun_config.bandwidth = config.bandwidth;
    tun_config.watermark_window_ms = config.min_window_ms;  // 使用最小窗口作为水位线窗口

    // TUN 统计信息
    TunStats tun_stats;

    // 启动 TUN 读取线程（遵循 RT-04：保留普通优先级，不设置实时调度）
    std::thread tun_thread([&]() {
        // RT-04: TUN 读取线程保留普通优先级（CFS），避免 FEC 编码 CPU 密集型导致系统死机
        // 不调用 set_thread_realtime_priority()
        TunError err = tun_reader_main_loop(&shared_state, tun_config,
                                            &g_shutdown, &tun_stats);

        if (err != TunError::OK) {
            std::cerr << "TUN 读取线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    // TX 配置
    TxConfig tx_config = {
        .mcs = config.mcs,
        .bandwidth = config.bandwidth,
        .batch_size = 32,
        .inject_retries = 3,
        .inject_retry_delay_us = 100
    };

    // 启动 TX 线程
    TxStats tx_stats;
    std::thread tx_thread([&]() {
        // 设置实时优先级 90
        if (!set_thread_realtime_priority(ThreadPriority::TX_WORKER, "tx_worker")) {
            std::cerr << "警告: 无法设置 TX 线程实时优先级" << std::endl;
        }

        // TODO: 需要实际的 pcap 句柄，目前使用 nullptr 作为占位符
        pcap_t* pcap_handle = nullptr;

        TxError err = tx_main_loop(&shared_state, pcap_handle,
                                   tx_config, &g_shutdown, &tx_stats);

        if (err != TxError::OK) {
            std::cerr << "TX 线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    std::cout << "服务端模式运行中，按 Ctrl+C 退出..." << std::endl;

    // 等待所有线程
    rx_thread.join();
    tun_thread.join();
    tx_thread.join();

    // 输出统计信息
    std::cout << "TUN 统计: 读取=" << tun_stats.packets_read.load()
              << ", 丢弃=" << tun_stats.packets_dropped.load()
              << ", FEC编码=" << tun_stats.fec_blocks_encoded.load()
              << ", 反压事件=" << tun_stats.backpressure_events.load() << std::endl;
    std::cout << "TX 统计: 发送=" << tx_stats.packets_sent.load()
              << ", 失败=" << tx_stats.packets_failed.load()
              << ", Token过期=" << tx_stats.token_expired.load() << std::endl;

    std::cout << "服务端模式退出" << std::endl;
    return 0;
}

int run_client(const Config& config) {
    ThreadSharedState shared_state;

    // RX 配置
    RxConfig rx_config = {
        .interface = config.interface.c_str(),
        .channel = config.channel,
        .pcap_timeout_ms = 10,
        .local_node_id = config.node_id,
        .is_server = false
    };

    // 启动 RX 线程
    std::thread rx_thread([&]() {
        // 设置实时优先级 99
        if (!set_thread_realtime_priority(ThreadPriority::CONTROL_PLANE, "rx_demux")) {
            std::cerr << "警告: 无法设置 RX 线程实时优先级" << std::endl;
        }

        RxStats stats;
        RxError err = rx_demux_main_loop(&shared_state, rx_config, &g_shutdown, &stats);

        if (err != RxError::OK) {
            std::cerr << "RX 线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    // TUN 配置（遵循 CLAUDE.md：txqueuelen 设置为 100，极小水池反压）
    TunConfig tun_config;
    tun_config.tun_name = config.tun_name;
    tun_config.txqueuelen = 100;  // 极小水池反压
    tun_config.fec = config.fec;
    tun_config.mcs = config.mcs;
    tun_config.bandwidth = config.bandwidth;
    tun_config.watermark_window_ms = config.min_window_ms;  // 使用最小窗口作为水位线窗口

    // TUN 统计信息
    TunStats tun_stats;

    // 启动 TUN 读取线程（遵循 RT-04：保留普通优先级，不设置实时调度）
    std::thread tun_thread([&]() {
        // RT-04: TUN 读取线程保留普通优先级（CFS），避免 FEC 编码 CPU 密集型导致系统死机
        // 不调用 set_thread_realtime_priority()
        TunError err = tun_reader_main_loop(&shared_state, tun_config,
                                            &g_shutdown, &tun_stats);

        if (err != TunError::OK) {
            std::cerr << "TUN 读取线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    // TX 配置
    TxConfig tx_config = {
        .mcs = config.mcs,
        .bandwidth = config.bandwidth,
        .batch_size = 32,
        .inject_retries = 3,
        .inject_retry_delay_us = 100
    };

    // 启动 TX 线程
    TxStats tx_stats;
    std::thread tx_thread([&]() {
        // 设置实时优先级 90
        if (!set_thread_realtime_priority(ThreadPriority::TX_WORKER, "tx_worker")) {
            std::cerr << "警告: 无法设置 TX 线程实时优先级" << std::endl;
        }

        // TODO: 需要实际的 pcap 句柄，目前使用 nullptr 作为占位符
        pcap_t* pcap_handle = nullptr;

        TxError err = tx_main_loop(&shared_state, pcap_handle,
                                   tx_config, &g_shutdown, &tx_stats);

        if (err != TxError::OK) {
            std::cerr << "TX 线程异常退出: " << static_cast<int>(err) << std::endl;
        }
    });

    std::cout << "客户端模式运行中，按 Ctrl+C 退出..." << std::endl;

    // 等待所有线程
    rx_thread.join();
    tun_thread.join();
    tx_thread.join();

    // 输出统计信息
    std::cout << "TUN 统计: 读取=" << tun_stats.packets_read.load()
              << ", 丢弃=" << tun_stats.packets_dropped.load()
              << ", FEC编码=" << tun_stats.fec_blocks_encoded.load()
              << ", 反压事件=" << tun_stats.backpressure_events.load() << std::endl;
    std::cout << "TX 统计: 发送=" << tx_stats.packets_sent.load()
              << ", 失败=" << tx_stats.packets_failed.load()
              << ", Token过期=" << tx_stats.token_expired.load() << std::endl;

    std::cout << "客户端模式退出" << std::endl;
    return 0;
}
