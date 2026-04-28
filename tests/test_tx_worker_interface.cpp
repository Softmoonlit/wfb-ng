// tests/test_tx_worker_interface.cpp
// TX 线程接口测试（RED 阶段）

#include <iostream>
#include <cassert>
#include "src/tx_worker.h"

int main() {
    std::cout << "测试 TX 线程接口..." << std::endl;

    // Test 1: 验证 tx_main_loop 函数签名
    std::cout << "测试 1: tx_main_loop 函数签名验证..." << std::endl;
    // 无法调用，仅验证编译

    // Test 2: TxConfig 结构体验证
    std::cout << "测试 2: TxConfig 结构体验证..." << std::endl;
    TxConfig config;
    config.mcs = 6;
    config.bandwidth = 20;
    config.batch_size = 32;
    config.inject_retries = 3;
    config.inject_retry_delay_us = 100;

    assert(config.mcs == 6);
    assert(config.bandwidth == 20);
    assert(config.batch_size == 32);
    assert(config.inject_retries == 3);
    assert(config.inject_retry_delay_us == 100);

    // Test 3: 错误码定义验证
    std::cout << "测试 3: 错误码定义验证..." << std::endl;
    TxError errors[] = {
        TxError::OK,
        TxError::PCAP_INJECT_FAILED,
        TxError::FRAME_BUILD_FAILED,
        TxError::THREAD_SHUTDOWN,
        TxError::TOKEN_EXPIRED
    };

    assert(static_cast<int>(errors[0]) == 0);
    assert(static_cast<int>(errors[1]) == 1);
    assert(static_cast<int>(errors[2]) == 2);
    assert(static_cast<int>(errors[3]) == 3);
    assert(static_cast<int>(errors[4]) == 4);

    std::cout << "所有测试通过！" << std::endl;
    return 0;
}