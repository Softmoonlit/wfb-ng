// tests/test_radiotap_template_standalone.cpp
// Radiotap 双模板分流系统单元测试（独立版本）
// 此文件用于验证核心逻辑，完整测试需要在 Linux 环境中运行

#include <cassert>
#include <iostream>
#include <cstring>
#include <vector>
#include <cstdint>

// Mock radiotap_header_t 结构
typedef struct {
    std::vector<uint8_t> header;
    uint8_t stbc;
    bool ldpc;
    bool short_gi;
    uint8_t bandwidth;
    uint8_t mcs_index;
    bool vht_mode;
    uint8_t vht_nss;
} radiotap_header_t;

// Mock init_radiotap_header 函数
radiotap_header_t init_radiotap_header(uint8_t stbc,
                                       bool ldpc,
                                       bool short_gi,
                                       uint8_t bandwidth,
                                       uint8_t mcs_index,
                                       bool vht_mode,
                                       uint8_t vht_nss) {
    radiotap_header_t hdr;
    hdr.stbc = stbc;
    hdr.ldpc = ldpc;
    hdr.short_gi = short_gi;
    hdr.bandwidth = bandwidth;
    hdr.mcs_index = mcs_index;
    hdr.vht_mode = vht_mode;
    hdr.vht_nss = vht_nss;
    return hdr;
}

// 全局变量定义（模拟实现）
radiotap_header_t g_control_radiotap;
radiotap_header_t g_data_radiotap;

// 初始化双模板
void init_radiotap_templates(uint8_t data_mcs_index, uint8_t data_bandwidth) {
    // 初始化控制帧模板：固定使用 MCS 0 + 20MHz
    g_control_radiotap = init_radiotap_header(
        /*stbc=*/0,
        /*ldpc=*/false,
        /*short_gi=*/false,
        /*bandwidth=*/20,      // 20MHz 频宽
        /*mcs_index=*/0,       // MCS 0（理论速率 6Mbps）
        /*vht_mode=*/false,
        /*vht_nss=*/1
    );

    // 初始化数据帧模板：基于 CLI 参数
    g_data_radiotap = init_radiotap_header(
        /*stbc=*/0,
        /*ldpc=*/false,
        /*short_gi=*/true,     // 启用 short GI 提高吞吐量
        /*bandwidth=*/data_bandwidth,
        /*mcs_index=*/data_mcs_index,
        /*vht_mode=*/false,
        /*vht_nss=*/1
    );
}

// 根据帧类型返回对应的模板指针（零延迟切换）
radiotap_header_t* get_radiotap_template(bool is_control_frame) {
    return is_control_frame ? &g_control_radiotap : &g_data_radiotap;
}

// 测试 1: 验证控制帧模板初始化
void test_control_template() {
    // 初始化模板（数据帧使用 MCS 4, 20MHz 作为测试）
    init_radiotap_templates(4, 20);

    // 验证控制帧模板
    radiotap_header_t* ctrl_template = get_radiotap_template(true);
    assert(ctrl_template != nullptr);
    assert(ctrl_template->mcs_index == 0);    // MCS 0
    assert(ctrl_template->bandwidth == 20);    // 20MHz

    std::cout << "PASS: 控制帧模板正确（MCS 0, 20MHz）" << std::endl;
}

// 测试 2: 验证数据帧模板初始化
void test_data_template() {
    // 验证数据帧模板
    radiotap_header_t* data_template = get_radiotap_template(false);
    assert(data_template != nullptr);
    assert(data_template->mcs_index == 4);     // MCS 4（测试参数）
    assert(data_template->bandwidth == 20);    // 20MHz（测试参数）

    std::cout << "PASS: 数据帧模板正确（MCS 4, 20MHz）" << std::endl;
}

// 测试 3: 验证模板切换
void test_template_switching() {
    // 验证模板切换
    radiotap_header_t* ctrl = get_radiotap_template(true);
    radiotap_header_t* data = get_radiotap_template(false);
    assert(ctrl != data);  // 不同的模板
    assert(ctrl == &g_control_radiotap);
    assert(data == &g_data_radiotap);

    std::cout << "PASS: 模板切换正确" << std::endl;
}

// 测试 4: 验证全局变量可访问
void test_global_access() {
    // 验证全局变量可直接访问
    assert(g_control_radiotap.mcs_index == 0);
    assert(g_control_radiotap.bandwidth == 20);

    std::cout << "PASS: 全局变量可访问" << std::endl;
}

int main() {
    std::cout << "=== Radiotap 双模板分流系统单元测试（独立版本） ===" << std::endl;

    test_control_template();
    test_data_template();
    test_template_switching();
    test_global_access();

    std::cout << std::endl;
    std::cout << "所有 Radiotap 模板测试通过！" << std::endl;
    return 0;
}
