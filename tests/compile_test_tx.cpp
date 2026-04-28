// tests/compile_test_tx.cpp
// 编译测试 TX 线程

#include "src/tx_worker.h"
#include <iostream>

int main() {
    std::cout << "TX 线程接口编译测试..." << std::endl;

    // 测试配置
    TxConfig config;
    config.mcs = 6;
    config.bandwidth = 20;
    config.batch_size = 32;
    config.inject_retries = 3;
    config.inject_retry_delay_us = 100;

    // 测试统计
    TxStats stats;

    std::cout << "配置验证: MCS=" << config.mcs
              << ", 带宽=" << config.bandwidth << "MHz"
              << ", 批量大小=" << config.batch_size << std::endl;

    std::cout << "编译测试通过!" << std::endl;
    return 0;
}