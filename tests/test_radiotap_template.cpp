// tests/test_radiotap_template.cpp
// Radiotap 双模板分流系统单元测试：验证模板初始化和切换正确性

#include <cassert>
#include <iostream>
#include <cstring>
#include "../src/radiotap_template.h"

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
    std::cout << "=== Radiotap 双模板分流系统单元测试 ===" << std::endl;

    test_control_template();
    test_data_template();
    test_template_switching();
    test_global_access();

    std::cout << std::endl;
    std::cout << "所有 Radiotap 模板测试通过！" << std::endl;
    return 0;
}
