// tests/test_scheduler.cpp
#include <cassert>
#include "../src/server_scheduler.h"

void test_scheduler_transitions() {
    ServerScheduler scheduler;
    scheduler.init_node(1);

    // 测试状态 III：高负载贪突发 (Additive Increase)
    // 节点 1 耗尽了分配的 100ms
    uint16_t next_window = scheduler.calculate_next_window(1, 100, 100);
    // 应当自增进攻扩窗步长 (100ms) = 200ms
    assert(next_window == 200);

    // 测试状态 I：极速枯竭与睡眠 Watchdog 保护
    // 节点 1 此次仅消耗 2ms (小于 5ms)
    next_window = scheduler.calculate_next_window(1, 2, 200);
    // 首次静默，Watchdog 拦截，维持原有窗口
    assert(next_window == 200);

    // 连续 3 次静默降级为 MIN_WINDOW (50ms)
    scheduler.calculate_next_window(1, 2, 200);
    next_window = scheduler.calculate_next_window(1, 2, 200);
    assert(next_window == 50);
}

int main() {
    test_scheduler_transitions();
    return 0;
}
