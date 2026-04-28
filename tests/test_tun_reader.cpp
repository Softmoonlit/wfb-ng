// tests/test_tun_reader.cpp
// TUN 读取线程单元测试：验证接口定义和函数签名的正确性

#include <cassert>
#include <iostream>
#include <string>
#include <atomic>
#include "../src/tun_reader.h"
#include "../src/threads.h"
#include "../src/config.h"

// 测试 1: tun_reader_main_loop 函数签名验证
void test_function_signature() {
    std::cout << "测试 1: tun_reader_main_loop 函数签名验证..." << std::endl;

    // 测试函数存在且可调用
    ThreadSharedState shared_state;
    TunConfig config;
    config.tun_name = "test0";
    config.txqueuelen = 100;
    config.fec.n = 12;
    config.fec.k = 8;
    config.mcs = 0;
    config.bandwidth = 20;
    config.watermark_window_ms = 50;
    std::atomic<bool> shutdown(false);
    TunStats stats;

    // 测试函数签名正确（编译时检查）
    TunError (*func_ptr)(ThreadSharedState*, const TunConfig&, std::atomic<bool>*, TunStats*) = tun_reader_main_loop;
    (void)func_ptr;  // 避免未使用警告

    std::cout << "PASS: tun_reader_main_loop 函数签名正确" << std::endl;
}

// 测试 2: TunConfig 结构体验证
void test_tun_config_structure() {
    std::cout << "测试 2: TunConfig 结构体验证..." << std::endl;

    // 测试默认值
    TunConfig config;
    assert(config.tun_name == "");
    assert(config.txqueuelen == 0);
    assert(config.fec.n == 12);
    assert(config.fec.k == 8);
    assert(config.mcs == 0);
    assert(config.bandwidth == 0);
    assert(config.watermark_window_ms == 0);

    // 测试初始化
    TunConfig config2;
    config2.tun_name = "wfb0";
    config2.txqueuelen = 100;
    config2.fec.n = 16;
    config2.fec.k = 10;
    config2.mcs = 4;
    config2.bandwidth = 40;
    config2.watermark_window_ms = 50;

    assert(config2.tun_name == "wfb0");
    assert(config2.txqueuelen == 100);
    assert(config2.fec.n == 16);
    assert(config2.fec.k == 10);
    assert(config2.mcs == 4);
    assert(config2.bandwidth == 40);
    assert(config2.watermark_window_ms == 50);

    std::cout << "PASS: TunConfig 结构体定义正确" << std::endl;
}

// 测试 3: 错误码定义验证
void test_tun_error_enum() {
    std::cout << "测试 3: 错误码定义验证..." << std::endl;

    // 测试枚举值
    assert(static_cast<int>(TunError::OK) == 0);
    assert(static_cast<int>(TunError::TUN_OPEN_FAILED) == 1);
    assert(static_cast<int>(TunError::TUN_IOCTL_FAILED) == 2);
    assert(static_cast<int>(TunError::TXQUEUELEN_SET_FAILED) == 3);
    assert(static_cast<int>(TunError::FEC_INIT_FAILED) == 4);
    assert(static_cast<int>(TunError::THREAD_SHUTDOWN) == 5);
    assert(static_cast<int>(TunError::READ_ERROR) == 6);
    assert(static_cast<int>(TunError::UNKNOWN_ERROR) == 7);

    // 测试枚举数量
    TunError errors[] = {
        TunError::OK,
        TunError::TUN_OPEN_FAILED,
        TunError::TUN_IOCTL_FAILED,
        TunError::TXQUEUELEN_SET_FAILED,
        TunError::FEC_INIT_FAILED,
        TunError::THREAD_SHUTDOWN,
        TunError::READ_ERROR,
        TunError::UNKNOWN_ERROR
    };
    assert(sizeof(errors) / sizeof(errors[0]) == 8);

    std::cout << "PASS: TunError 枚举定义正确，共 8 个错误码" << std::endl;
}

// 测试 4: TunStats 结构体验证
void test_tun_stats_structure() {
    std::cout << "测试 4: TunStats 结构体验证..." << std::endl;

    TunStats stats;

    // 测试默认值
    assert(stats.packets_read.load() == 0u);
    assert(stats.packets_dropped.load() == 0u);
    assert(stats.fec_blocks_encoded.load() == 0u);
    assert(stats.backpressure_events.load() == 0u);
    assert(stats.read_errors.load() == 0u);

    // 测试原子操作
    stats.packets_read.fetch_add(1);
    assert(stats.packets_read.load() == 1u);

    stats.packets_dropped.fetch_add(2);
    assert(stats.packets_dropped.load() == 2u);

    stats.fec_blocks_encoded.fetch_add(3);
    assert(stats.fec_blocks_encoded.load() == 3u);

    stats.backpressure_events.fetch_add(4);
    assert(stats.backpressure_events.load() == 4u);

    stats.read_errors.fetch_add(5);
    assert(stats.read_errors.load() == 5u);

    std::cout << "PASS: TunStats 结构体原子操作正常" << std::endl;
}

// 测试 5: 辅助函数签名验证
void test_helper_function_signatures() {
    std::cout << "测试 5: 辅助函数签名验证..." << std::endl;

    // 测试 create_tun_device 函数签名
    int (*create_tun_ptr)(const std::string&, char*) = create_tun_device;
    (void)create_tun_ptr;

    // 测试 set_tun_txqueuelen 函数签名
    bool (*set_txqueuelen_ptr)(const std::string&, int) = set_tun_txqueuelen;
    (void)set_txqueuelen_ptr;

    std::cout << "PASS: 辅助函数签名正确" << std::endl;
}

int main() {
    std::cout << "=== TUN 读取线程接口测试 ===\n" << std::endl;

    try {
        // 注意：由于 tun_reader.h 尚未创建，测试将在编译时失败（TDD RED 阶段）
        test_function_signature();
        test_tun_config_structure();
        test_tun_error_enum();
        test_tun_stats_structure();
        test_helper_function_signatures();

        std::cout << "\n=== 所有测试通过 ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知错误" << std::endl;
        return 1;
    }
}