#include "config.h"
#include "error_handler.h"
#include "threads.h"

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
    std::cout << "服务端模式待实现" << std::endl;

    // 后续计划填充
    // 1. 创建共享状态
    // 2. 启动控制线程、发射线程、TUN读取线程
    // 3. 等待关闭信号
    // 4. 清理资源

    // 临时实现：等待关闭信号
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "服务端模式退出" << std::endl;
    return 0;
}

int run_client(const Config& config) {
    std::cout << "客户端模式待实现" << std::endl;

    // 后续计划填充
    // 1. 创建共享状态
    // 2. 启动控制线程、发射线程、TUN读取线程
    // 3. 等待关闭信号
    // 4. 清理资源

    // 临时实现：等待关闭信号
    while (!g_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "客户端模式退出" << std::endl;
    return 0;
}