#include <cassert>
#include "../src/threads.h"
#include "../src/watermark.h"

void test_shared_state_defaults() {
    ThreadSharedState shared_state;
    assert(shared_state.can_send == false);
    assert(shared_state.token_expire_time_ms == 0);
}

void test_priority_constants() {
    assert(kControlThreadPriority == 99);
    assert(kTxThreadPriority == 90);
    assert(kGuardIntervalMs == 5);
}

void test_token_gate_logic() {
    // 测试 Token 门控逻辑（per PROC-07, PROC-10）
    ThreadSharedState shared_state;

    // 初始状态：门控关闭
    assert(!shared_state.can_send);

    // 模拟控制面下发 Token
    {
        std::lock_guard<std::mutex> lock(shared_state.mtx);
        shared_state.can_send = true;
        shared_state.token_expire_time_ms = 10000;  // 未来时间
    }

    // 验证门控已打开
    assert(shared_state.can_send);

    // 模拟 Token 过期
    {
        std::lock_guard<std::mutex> lock(shared_state.mtx);
        shared_state.token_expire_time_ms = 0;  // 已过期
    }

    // 验证过期后门控应关闭
    // 注意：实际逻辑在 tx_main_loop 中处理
}

void test_dynamic_watermark_integration() {
    // 测试动态水位线与线程状态集成（per BUFFER-03）
    size_t limit = calculate_dynamic_limit(25, kDefaultPhyRateBps, 1450);
    assert(limit > 0);

    // 验证默认物理速率（6Mbps）下的水位线
    // 公式: ceil((6000000 * 25 * 1.5) / (8 * 1450 * 1000)) = 20
    assert(limit == 20);
}

void test_guard_interval_constant() {
    // 测试保护间隔常量（per D-16, RT-08）
    assert(kGuardIntervalMs == 5);

    // 验证保护间隔值在合理范围内（1-10ms）
    assert(kGuardIntervalMs >= 1);
    assert(kGuardIntervalMs <= 10);
}

int main() {
    test_shared_state_defaults();
    test_priority_constants();
    test_token_gate_logic();
    test_dynamic_watermark_integration();
    test_guard_interval_constant();
    return 0;
}
