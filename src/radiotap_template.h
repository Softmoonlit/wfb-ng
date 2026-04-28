// src/radiotap_template.h
// Radiotap 双模板分流系统：控制帧最低 MCS 模板和数据帧高阶 MCS 模板
#pragma once

#include <cstdint>
#include <vector>

// IEEE 802.11 Radiotap 头部结构
struct radiotap_header_t {
    uint8_t  version;
    uint8_t  pad;
    uint16_t len;
    uint32_t present;
    uint32_t present_ext[3];
    uint8_t  data[56];  // 变长数据区域
} __attribute__((packed));

// 控制帧模板：固定使用 MCS 0 + 20MHz（理论速率 6Mbps），确保全网覆盖
extern radiotap_header_t g_control_radiotap;

// 数据帧模板：基于 CLI 参数动态初始化
extern radiotap_header_t g_data_radiotap;

// 初始化双模板（在进程启动时调用）
void init_radiotap_templates(uint8_t data_mcs_index, uint8_t data_bandwidth);

// 根据帧类型返回对应的模板指针（零延迟切换）
radiotap_header_t* get_radiotap_template(bool is_control_frame);

// C++ 风格的 Radiotap 模板类（用于新架构）
class RadiotapTemplate {
public:
    RadiotapTemplate() = default;

    // 初始化控制帧模板（最低 MCS）
    void init_for_control() {
        // 最小 Radiotap 头部：版本 0，长度 8，无可选字段
        template_data_.clear();
        template_data_.push_back(0x00);  // version
        template_data_.push_back(0x00);  // pad
        template_data_.push_back(0x08);  // length (low)
        template_data_.push_back(0x00);  // length (high)
        template_data_.push_back(0x00);  // present flags
        template_data_.push_back(0x00);
        template_data_.push_back(0x00);
        template_data_.push_back(0x00);
        initialized_ = true;
    }

    // 初始化数据帧模板
    // @param mcs MCS 索引 (0-8)
    // @param bandwidth_mhz 频宽 (20 或 40 MHz)
    void init_for_data(int mcs, int bandwidth_mhz) {
        // 简化实现：创建基本 Radiotap 头部
        (void)mcs;
        (void)bandwidth_mhz;
        init_for_control();  // 简化：使用相同模板
    }

    // 获取模板数据指针
    const uint8_t* get_template() const {
        return template_data_.empty() ? nullptr : template_data_.data();
    }

    // 获取模板长度
    uint16_t get_length() const {
        return static_cast<uint16_t>(template_data_.size());
    }

private:
    std::vector<uint8_t> template_data_;
    bool initialized_ = false;
};
