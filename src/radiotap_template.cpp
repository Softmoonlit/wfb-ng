// src/radiotap_template.cpp
// Radiotap 双模板分流系统实现

#include "radiotap_template.h"

// 全局静态变量定义
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
    // 根据帧类型返回对应的模板指针（零延迟切换）
    return is_control_frame ? &g_control_radiotap : &g_data_radiotap;
}
