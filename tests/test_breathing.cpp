// tests/test_breathing.cpp
// 呼吸周期状态机单元测试
#include <cassert>
#include <iostream>
#include <ctime>
#include "../src/breathing_cycle.h"

// 测试初始相位
void test_initial_phase() {
    BreathingCycle cycle;

    // 验证初始相位为 EXHALE
    assert(cycle.get_current_phase() == BreathingPhase::EXHALE);

    std::cout << "test_initial_phase: PASSED" << std::endl;
}

// 测试呼出到吸气切换
void test_exhale_to_inhale_transition() {
    BreathingCycle cycle;
    uint64_t base_time = 1000;  // 基准时间

    // 首次调用初始化
    cycle.update(base_time);
    assert(cycle.get_current_phase() == BreathingPhase::EXHALE);

    // 799ms 后仍在 EXHALE
    assert(cycle.update(base_time + 799) == BreathingPhase::EXHALE);

    // 800ms 后切换到 INHALE
    assert(cycle.update(base_time + 800) == BreathingPhase::INHALE);

    std::cout << "test_exhale_to_inhale_transition: PASSED" << std::endl;
}

// 测试吸气到呼出切换
void test_inhale_to_exhale_transition() {
    BreathingCycle cycle;
    uint64_t base_time = 1000;

    // 先切换到 INHALE
    cycle.update(base_time);
    cycle.update(base_time + 800);
    assert(cycle.get_current_phase() == BreathingPhase::INHALE);

    uint64_t inhale_start = base_time + 800;

    // 199ms 后仍在 INHALE
    assert(cycle.update(inhale_start + 199) == BreathingPhase::INHALE);

    // 200ms 后切换回 EXHALE
    assert(cycle.update(inhale_start + 200) == BreathingPhase::EXHALE);

    std::cout << "test_inhale_to_exhale_transition: PASSED" << std::endl;
}

// 测试周期时长
void test_cycle_duration() {
    BreathingCycle cycle;

    // 验证周期总时长 = 800 + 200 = 1000ms
    assert(cycle.get_cycle_duration_ms() == 1000);

    // 验证各阶段时长
    assert(cycle.get_exhale_duration_ms() == 800);
    assert(cycle.get_inhale_duration_ms() == 200);

    std::cout << "test_cycle_duration: PASSED" << std::endl;
}

// 测试完整呼吸周期
void test_full_cycle() {
    BreathingCycle cycle;
    uint64_t base_time = 0;

    // 初始化
    BreathingPhase phase = cycle.update(base_time);
    assert(phase == BreathingPhase::EXHALE);

    // 第一个 EXHALE 阶段
    phase = cycle.update(base_time + 100);
    assert(phase == BreathingPhase::EXHALE);

    phase = cycle.update(base_time + 799);
    assert(phase == BreathingPhase::EXHALE);

    // 切换到 INHALE
    phase = cycle.update(base_time + 800);
    assert(phase == BreathingPhase::INHALE);

    // INHALE 阶段
    phase = cycle.update(base_time + 900);
    assert(phase == BreathingPhase::INHALE);

    phase = cycle.update(base_time + 999);
    assert(phase == BreathingPhase::INHALE);

    // 切换回 EXHALE（1000ms 完成一个完整周期）
    phase = cycle.update(base_time + 1000);
    assert(phase == BreathingPhase::EXHALE);

    // 第二个周期
    phase = cycle.update(base_time + 1500);
    assert(phase == BreathingPhase::EXHALE);

    phase = cycle.update(base_time + 1800);
    assert(phase == BreathingPhase::INHALE);

    phase = cycle.update(base_time + 2000);
    assert(phase == BreathingPhase::EXHALE);

    std::cout << "test_full_cycle: PASSED" << std::endl;
}

// 测试吸气阶段 NACK 窗口
void test_inhale_nack_windows() {
    BreathingCycle cycle;

    // 设置 10 个节点
    cycle.set_num_nodes(10);
    assert(cycle.get_num_nodes() == 10);

    // 执行吸气阶段（不测量实际时间，仅验证逻辑正确）
    // 注意：此测试会实际执行保护间隔休眠
    cycle.execute_inhale_phase();

    // 验证吸气阶段时间常量
    assert(cycle.get_nack_window_ms() == 25);
    assert(cycle.get_guard_interval_ms() == 5);

    std::cout << "test_inhale_nack_windows: PASSED" << std::endl;
}

// 测试节点数量设置
void test_set_num_nodes() {
    BreathingCycle cycle;

    // 默认 10 节点
    assert(cycle.get_num_nodes() == 10);

    // 设置为 5 节点
    cycle.set_num_nodes(5);
    assert(cycle.get_num_nodes() == 5);

    // 设置为 20 节点
    cycle.set_num_nodes(20);
    assert(cycle.get_num_nodes() == 20);

    // 边界值：0 应该被忽略
    cycle.set_num_nodes(0);
    assert(cycle.get_num_nodes() == 20);  // 保持之前的值

    // 边界值：256 应该被忽略（超出 uint8_t 范围）
    cycle.set_num_nodes(256);
    assert(cycle.get_num_nodes() == 20);  // 保持之前的值

    // 边界值：255 应该被接受
    cycle.set_num_nodes(255);
    assert(cycle.get_num_nodes() == 255);

    std::cout << "test_set_num_nodes: PASSED" << std::endl;
}

int main() {
    std::cout << "=== 呼吸周期状态机测试 ===" << std::endl;

    test_initial_phase();
    test_exhale_to_inhale_transition();
    test_inhale_to_exhale_transition();
    test_cycle_duration();
    test_full_cycle();
    test_inhale_nack_windows();
    test_set_num_nodes();

    std::cout << "=== 所有测试通过 ===" << std::endl;
    return 0;
}
