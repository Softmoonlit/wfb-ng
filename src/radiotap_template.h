// src/radiotap_template.h
// Radiotap 双模板分流系统：控制帧最低 MCS 模板和数据帧高阶 MCS 模板
#pragma once

#include "tx.hpp"  // 复用 radiotap_header_t 结构

// 控制帧模板：固定使用 MCS 0 + 20MHz（理论速率 6Mbps），确保全网覆盖
extern radiotap_header_t g_control_radiotap;

// 数据帧模板：基于 CLI 参数动态初始化
extern radiotap_header_t g_data_radiotap;

// 初始化双模板（在进程启动时调用）
void init_radiotap_templates(uint8_t data_mcs_index, uint8_t data_bandwidth);

// 根据帧类型返回对应的模板指针（零延迟切换）
radiotap_header_t* get_radiotap_template(bool is_control_frame);
