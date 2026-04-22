// tests/test_watermark.cpp
#include <cassert>
#include "../src/watermark.h"

void test_client_limit() {
    // 6Mbps、25ms、1450 字节 MTU 的基础场景，作为 Phase 2 反压闭环基线。
    size_t limit = calculate_dynamic_limit(25, 6000000, 1450);
    assert(limit == 20);
}

void test_higher_rate_limit() {
    // 更高物理速率下，动态水位线应随 phy_rate_bps 稳定上升。
    size_t limit = calculate_dynamic_limit(25, 12000000, 1450);
    assert(limit == 39);
}

int main() {
    test_client_limit();
    test_higher_rate_limit();
    return 0;
}
