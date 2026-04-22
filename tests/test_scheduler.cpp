// tests/test_scheduler.cpp
// 三态状态机调度器单元测试
#include <cassert>
#include <iostream>
#include "../src/server_scheduler.h"
#include "../src/aq_sq_manager.h"

// 测试辅助函数：打印测试结果
#define TEST_PASS(name) std::cout << "[PASS] " << name << std::endl
#define TEST_FAIL(name, detail) std::cout << "[FAIL] " << name << ": " << detail << std::endl

/**
 * 测试状态机初始状态
 * - 新节点初始状态为 STATE_I
 * - 初始窗口为 MIN_WINDOW (50ms)
 */
void test_state_machine_initial_state() {
    ServerScheduler scheduler;
    scheduler.init_node(1);

    // 初始化后首次调用 calculate_next_window
    // 实际使用 < 5ms，首次静默
    uint16_t next_window = scheduler.calculate_next_window(1, 2, 100);

    // Watchdog 保护：首次静默保持原窗口
    assert(next_window == 100);
    TEST_PASS("test_state_machine_initial_state");
}

/**
 * 测试状态 III：高负载贪突发 (Additive Increase)
 * - 节点耗尽 100ms 授权窗口
 * - 下一窗口 = min(100 + 100, MAX_WINDOW) = 200ms
 * - 状态跃迁到 STATE_III
 */
void test_state_iii_additive_increase() {
    ServerScheduler scheduler;
    scheduler.init_node(1);
    scheduler.set_num_nodes(10);  // 10 节点，MAX_WINDOW = 300ms

    // 节点耗尽 100ms 授权窗口
    uint16_t next_window = scheduler.calculate_next_window(1, 100, 100);

    // 应当扩窗: 100 + 100 = 200ms
    assert(next_window == 200);

    // 继续耗尽，应当继续扩窗
    next_window = scheduler.calculate_next_window(1, 200, 200);
    assert(next_window == 300);  // 200 + 100 = 300ms

    // 再扩窗，应当被 MAX_WINDOW 限制
    next_window = scheduler.calculate_next_window(1, 300, 300);
    assert(next_window == 300);  // min(300 + 100, 300) = 300ms

    TEST_PASS("test_state_iii_additive_increase");
}

/**
 * 测试状态 II：需量拟合 (Demand-Fitting)
 * - 节点使用 60ms（小于 100ms 授权）
 * - 下一窗口 = max(50, 60 + 50) = 110ms
 * - 状态跃迁到 STATE_II
 */
void test_state_ii_demand_fitting() {
    ServerScheduler scheduler;
    scheduler.init_node(1);

    // 节点使用 60ms（小于授权 100ms）
    uint16_t next_window = scheduler.calculate_next_window(1, 60, 100);

    // 应当拟合: max(50, 60 + 50) = 110ms
    assert(next_window == 110);

    // 再测试边界情况：使用 10ms
    next_window = scheduler.calculate_next_window(1, 10, 100);
    // 应当拟合: max(50, 10 + 50) = 60ms
    assert(next_window == 60);

    // 边界情况：使用 5ms（刚好等于阈值）
    next_window = scheduler.calculate_next_window(1, 5, 100);
    // 应当拟合: max(50, 5 + 50) = 55ms
    assert(next_window == 55);

    TEST_PASS("test_state_ii_demand_fitting");
}

/**
 * 测试 Watchdog 保护
 * - 节点连续 2 次静默（< 5ms）
 * - 窗口保持不变（Watchdog 保护）
 * - 第 3 次静默，窗口降级到 MIN_WINDOW
 */
void test_watchdog_protection() {
    ServerScheduler scheduler;
    scheduler.init_node(1);

    // 第一次静默
    uint16_t next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 100);  // Watchdog 保护

    // 第二次静默
    next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 100);  // Watchdog 保护

    // 第三次静默
    next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 50);   // 降级到 MIN_WINDOW

    TEST_PASS("test_watchdog_protection");
}

/**
 * 测试 MAX_WINDOW 动态计算
 * - 设置 5 节点，MAX_WINDOW = 600ms
 * - 设置 10 节点，MAX_WINDOW = 300ms
 * - 设置 20 节点，MAX_WINDOW = 150ms
 */
void test_max_window_dynamic() {
    ServerScheduler scheduler;

    // 5 节点
    scheduler.set_num_nodes(5);
    scheduler.init_node(1);
    // 通过扩窗测试 MAX_WINDOW
    uint16_t next_window = scheduler.calculate_next_window(1, 500, 500);
    assert(next_window == 600);  // min(500 + 100, 600) = 600ms

    // 再扩窗，仍被 600ms 限制
    next_window = scheduler.calculate_next_window(1, 600, 600);
    assert(next_window == 600);

    // 10 节点
    scheduler.set_num_nodes(10);
    scheduler.init_node(2);
    next_window = scheduler.calculate_next_window(2, 300, 300);
    assert(next_window == 300);  // min(300 + 100, 300) = 300ms

    // 20 节点
    scheduler.set_num_nodes(20);
    scheduler.init_node(3);
    next_window = scheduler.calculate_next_window(3, 150, 150);
    assert(next_window == 150);  // min(150 + 100, 150) = 150ms

    TEST_PASS("test_max_window_dynamic");
}

/**
 * 测试完整状态跃迁序列
 * - 初始 STATE_I → 状态 II（首次活动）
 * - 状态 II → 状态 III（耗尽窗口）
 * - 状态 III → 状态 I（连续 3 次静默）
 */
void test_state_transition_sequence() {
    ServerScheduler scheduler;
    scheduler.init_node(1);
    scheduler.set_num_nodes(10);

    // 初始状态：3 次静默后降级到 STATE_I
    scheduler.calculate_next_window(1, 2, 100);  // 静默 1
    scheduler.calculate_next_window(1, 2, 100);  // 静默 2
    uint16_t next_window = scheduler.calculate_next_window(1, 2, 100);  // 静默 3
    assert(next_window == 50);  // MIN_WINDOW

    // STATE_I → STATE_II：首次活动
    next_window = scheduler.calculate_next_window(1, 30, 50);
    // 拟合: max(50, 30 + 50) = 80ms
    assert(next_window == 80);

    // STATE_II → STATE_III：耗尽窗口
    next_window = scheduler.calculate_next_window(1, 80, 80);
    // 扩窗: 80 + 100 = 180ms
    assert(next_window == 180);

    // 继续扩窗
    next_window = scheduler.calculate_next_window(1, 180, 180);
    // 扩窗: 180 + 100 = 280ms
    assert(next_window == 280);

    // 再次扩窗，接近上限
    next_window = scheduler.calculate_next_window(1, 280, 280);
    // 扩窗: min(280 + 100, 300) = 300ms
    assert(next_window == 300);

    // STATE_III → STATE_I：连续 3 次静默
    scheduler.calculate_next_window(1, 2, 300);  // 静默 1
    scheduler.calculate_next_window(1, 2, 300);  // 静默 2
    next_window = scheduler.calculate_next_window(1, 2, 300);  // 静默 3
    assert(next_window == 50);  // 回到 MIN_WINDOW

    TEST_PASS("test_state_transition_sequence");
}

/**
 * 测试 AQ/SQ 队列迁移集成
 * - 状态 I 断流后迁移到 SQ
 * - 状态 II/III 活动后迁移到 AQ
 */
void test_aq_sq_migration() {
    ServerScheduler scheduler;
    AqSqManager manager;
    scheduler.set_aq_sq_manager(&manager);
    scheduler.init_node(1);
    scheduler.set_num_nodes(10);

    // 初始化节点到 SQ
    manager.init_node(1);

    // 3 次静默后应当迁移到 SQ（已经在 SQ，验证不崩溃）
    scheduler.calculate_next_window(1, 2, 100);
    scheduler.calculate_next_window(1, 2, 100);
    scheduler.calculate_next_window(1, 2, 100);

    // 活动：应当迁移到 AQ
    scheduler.calculate_next_window(1, 50, 50);
    assert(manager.aq_size() == 1);
    assert(manager.sq_size() == 0);

    // 再次静默 3 次后迁移回 SQ
    scheduler.calculate_next_window(1, 2, 80);
    scheduler.calculate_next_window(1, 2, 80);
    scheduler.calculate_next_window(1, 2, 80);
    assert(manager.aq_size() == 0);
    assert(manager.sq_size() == 1);

    TEST_PASS("test_aq_sq_migration");
}

int main() {
    std::cout << "=== ServerScheduler 三态状态机测试 ===" << std::endl;

    test_state_machine_initial_state();
    test_state_iii_additive_increase();
    test_state_ii_demand_fitting();
    test_watchdog_protection();
    test_max_window_dynamic();
    test_state_transition_sequence();
    test_aq_sq_migration();

    std::cout << std::endl << "=== 所有测试通过 ===" << std::endl;
    return 0;
}
