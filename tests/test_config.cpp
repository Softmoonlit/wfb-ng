#include "config.h"
#include "error_handler.h"

#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <getopt.h>  // for optind

// Test 1: Config 默认值验证（FEC N=12, K=8）
void test_default_config() {
    Config config;
    std::cout << "Test 1: Config 默认值验证..." << std::endl;

    // 验证默认值
    assert(config.mode == Config::Mode::SERVER);
    assert(config.fec.n == 12);
    assert(config.fec.k == 8);
    assert(config.fec.redundancy() == 4);
    assert(config.channel == 6);
    assert(config.mcs == 0);
    assert(config.bandwidth == 20);
    assert(config.tun_name == "wfb0");
    assert(config.node_id == 0);
    assert(config.verbose == false);
    assert(config.min_window_ms == 50);
    assert(config.max_window_ms == 300);
    assert(config.step_up_ms == 100);
    assert(config.elastic_margin_ms == 50);
    assert(config.guard_interval_ms == 5);

    // 验证 FEC 有效性
    assert(config.fec.is_valid() == true);

    std::cout << "  ✓ 默认值验证通过" << std::endl;
}

// Test 2: Config 命令行解析验证
void test_command_line_parsing() {
    std::cout << "Test 2: 命令行解析验证..." << std::endl;

    // 模拟命令行参数
    const char* argv[] = {
        "wfb_core",
        "--mode", "server",
        "-i", "wlan0",
        "-c", "8",
        "-m", "2",
        "--bw", "40",
        "--tun", "wfb1",
        "--fec-n", "16",
        "--fec-k", "10",
        "-v"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // 重置 getopt 状态
    optind = 1;
    opterr = 0;

    Config config;
    bool result = parse_arguments(argc, const_cast<char**>(argv), config);

    assert(result == true);
    assert(config.mode == Config::Mode::SERVER);
    assert(config.interface == "wlan0");
    assert(config.channel == 8);
    assert(config.mcs == 2);
    assert(config.bandwidth == 40);
    assert(config.tun_name == "wfb1");
    assert(config.fec.n == 16);
    assert(config.fec.k == 10);
    assert(config.verbose == true);
    assert(config.fec.redundancy() == 6);
    assert(config.fec.is_valid() == true);

    std::cout << "  ✓ 命令行解析验证通过" << std::endl;
}

// Test 3: 错误处理宏重试逻辑验证
void test_error_handler_macros() {
    std::cout << "Test 3: 错误处理宏验证..." << std::endl;

    // 测试 RETRY_ON_ERROR 宏
    int call_count = 0;
    auto failing_func = [&call_count]() -> int {
        call_count++;
        if (call_count < 3) {
            return -1;  // 前两次失败
        }
        return 0;       // 第三次成功
    };

    // 最多重试5次，延迟10ms
    int result = RETRY_ON_ERROR(failing_func(), 5, 10);
    assert(result == 0);
    assert(call_count == 3);

    std::cout << "  ✓ 错误处理宏验证通过" << std::endl;
}

// Test 4: 配置验证函数
void test_config_validation() {
    std::cout << "Test 4: 配置验证函数测试..." << std::endl;

    Config config;

    // 空接口应该失败
    config.interface = "";
    assert(config.validate() == false);

    // 无效的 FEC 参数应该失败
    config.interface = "wlan0";
    config.fec.n = 5;
    config.fec.k = 10;  // n < k
    assert(config.validate() == false);

    // 客户端模式但没有 node_id 应该失败
    config.fec.n = 12;
    config.fec.k = 8;
    config.mode = Config::Mode::CLIENT;
    config.node_id = 0;  // 无效
    assert(config.validate() == false);

    // 有效的客户端配置
    config.node_id = 5;
    assert(config.validate() == true);

    std::cout << "  ✓ 配置验证函数测试通过" << std::endl;
}

// Test 5: 客户端模式解析
void test_client_mode_parsing() {
    std::cout << "Test 5: 客户端模式解析测试..." << std::endl;

    const char* argv[] = {
        "wfb_core",
        "--mode", "client",
        "-i", "wlan1",
        "--node-id", "3",
        "--fec-n", "14",
        "--fec-k", "9"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // 重置 getopt 状态
    optind = 1;
    opterr = 0;

    Config config;
    bool result = parse_arguments(argc, const_cast<char**>(argv), config);

    assert(result == true);
    assert(config.mode == Config::Mode::CLIENT);
    assert(config.interface == "wlan1");
    assert(config.node_id == 3);
    assert(config.fec.n == 14);
    assert(config.fec.k == 9);
    assert(config.fec.redundancy() == 5);
    assert(config.fec.is_valid() == true);
    assert(config.validate() == true);

    std::cout << "  ✓ 客户端模式解析测试通过" << std::endl;
}

int main() {
    std::cout << "=== 开始配置和错误处理测试 ===" << std::endl;

    try {
        test_default_config();
        test_command_line_parsing();
        test_error_handler_macros();
        test_config_validation();
        test_client_mode_parsing();

        std::cout << "\n=== 所有测试通过 ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n测试失败: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n未知测试失败" << std::endl;
        return 1;
    }
}