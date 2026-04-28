// tests/test_frame_build_safe.cpp
// 帧构建函数测试（RED 阶段）

#include <iostream>
#include <cassert>
#include <cstring>
#include "src/tx_worker.h"

// 声明要测试的函数
namespace {
// 从 tx_worker.cpp 中导出的函数声明
size_t build_frame_safe(const TxPacket& pkt,
                        const RadiotapTemplate& rtap_template,
                        std::vector<uint8_t>& frame);

int pcap_inject_retry(pcap_t* pcap_handle,
                      const uint8_t* data,
                      size_t len,
                      int max_retries,
                      uint32_t retry_delay_us);
}

// 使用头文件中的 RadiotapTemplate 类

int main() {
    std::cout << "测试帧构建函数..." << std::endl;

    // 创建模拟对象
    RadiotapTemplate rtap_template;
    rtap_template.init_for_data(6, 20);

    // 测试数据
    TxPacket test_pkt;
    test_pkt.data = std::vector<uint8_t>(100, 0xAA);
    test_pkt.len = 100;

    std::vector<uint8_t> frame;

    // 测试 1: 正常数据帧构建成功
    std::cout << "测试 1: 正常数据帧构建..." << std::endl;
    size_t result = build_frame_safe(test_pkt, rtap_template, frame);
    // 由于缺少函数实现，此测试会失败

    // 测试 2: 空数据包返回错误
    std::cout << "测试 2: 空数据包处理..." << std::endl;
    TxPacket empty_pkt;
    empty_pkt.len = 0;
    size_t empty_result = build_frame_safe(empty_pkt, rtap_template, frame);

    // 测试 3: 过大数据包返回错误
    std::cout << "测试 3: 过大数据包处理..." << std::endl;
    TxPacket oversize_pkt;
    oversize_pkt.data = std::vector<uint8_t>(2000, 0xBB);
    oversize_pkt.len = 2000;
    size_t oversize_result = build_frame_safe(oversize_pkt, rtap_template, frame);

    // 测试 4: Radiotap 头部正确追加
    std::cout << "测试 4: Radiotap 头部验证..." << std::endl;
    // 需要验证帧格式

    std::cout << "所有测试完成（预期失败）" << std::endl;
    return 0;
}