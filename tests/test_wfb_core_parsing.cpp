#include "config.h"
#include <getopt.h>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

// Test 1: --mode server 正确解析
void test_mode_server() {
    std::cout << "Test 1: --mode server 正确解析..." << std::endl;

    const char* argv[] = {
        "wfb_core",
        "--mode", "server",
        "-i", "wlan0"
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
    assert(config.validate() == true);

    std::cout << "  ✓ --mode server 解析通过" << std::endl;
}

// Test 2: --mode client --node-id 5 正确解析
void test_mode_client() {
    std::cout << "Test 2: --mode client --node-id 5 正确解析..." << std::endl;

    const char* argv[] = {
        "wfb_core",
        "--mode", "client",
        "-i", "wlan1",
        "--node-id", "5"
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
    assert(config.node_id == 5);
    assert(config.validate() == true);

    std::cout << "  ✓ --mode client 解析通过" << std::endl;
}

// Test 3: -i wlan0 -c 6 -m 0 正确解析
void test_network_params() {
    std::cout << "Test 3: -i wlan0 -c 6 -m 0 正确解析..." << std::endl;

    const char* argv[] = {
        "wfb_core",
        "--mode", "server",
        "-i", "wlan0",
        "-c", "6",
        "-m", "0"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // 重置 getopt 状态
    optind = 1;
    opterr = 0;

    Config config;
    bool result = parse_arguments(argc, const_cast<char**>(argv), config);

    assert(result == true);
    assert(config.interface == "wlan0");
    assert(config.channel == 6);
    assert(config.mcs == 0);
    assert(config.validate() == true);

    std::cout << "  ✓ 网络参数解析通过" << std::endl;
}

// Test 4: --fec-n 16 --fec-k 10 正确解析
void test_fec_params() {
    std::cout << "Test 4: --fec-n 16 --fec-k 10 正确解析..." << std::endl;

    const char* argv[] = {
        "wfb_core",
        "--mode", "server",
        "-i", "wlan0",
        "--fec-n", "16",
        "--fec-k", "10"
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    // 重置 getopt 状态
    optind = 1;
    opterr = 0;

    Config config;
    bool result = parse_arguments(argc, const_cast<char**>(argv), config);

    assert(result == true);
    assert(config.fec.n == 16);
    assert(config.fec.k == 10);
    assert(config.fec.redundancy() == 6);
    assert(config.fec.is_valid() == true);
    assert(config.validate() == true);

    std::cout << "  ✓ FEC 参数解析通过" << std::endl;
}

// Test 5: 无效参数返回错误
void test_invalid_params() {
    std::cout << "Test 5: 无效参数返回错误..." << std::endl;

    // 测试1: 缺少必需参数 -i
    {
        const char* argv[] = {
            "wfb_core",
            "--mode", "server"
        };
        int argc = sizeof(argv) / sizeof(argv[0]);

        optind = 1;
        opterr = 0;

        Config config;
        bool result = parse_arguments(argc, const_cast<char**>(argv), config);

        assert(result == false);  // 应该失败，缺少 -i
    }

    // 测试2: 无效的 FEC 参数
    {
        const char* argv[] = {
            "wfb_core",
            "--mode", "server",
            "-i", "wlan0",
            "--fec-n", "5",
            "--fec-k", "10"  // n < k, 无效
        };
        int argc = sizeof(argv) / sizeof(argv[0]);

        optind = 1;
        opterr = 0;

        Config config;
        bool result = parse_arguments(argc, const_cast<char**>(argv), config);

        assert(result == false);  // 应该失败，FEC 参数无效
    }

    // 测试3: 客户端模式缺少 node-id
    {
        const char* argv[] = {
            "wfb_core",
            "--mode", "client",
            "-i", "wlan0"
        };
        int argc = sizeof(argv) / sizeof(argv[0]);

        optind = 1;
        opterr = 0;

        Config config;
        bool result = parse_arguments(argc, const_cast<char**>(argv), config);

        assert(result == false);  // 应该失败，缺少 node-id
    }

    std::cout << "  ✓ 无效参数测试通过" << std::endl;
}

int main() {
    std::cout << "=== 开始 wfb_core 命令行参数解析测试 ===" << std::endl;

    try {
        test_mode_server();
        test_mode_client();
        test_network_params();
        test_fec_params();
        test_invalid_params();

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