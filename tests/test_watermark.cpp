// tests/test_watermark.cpp
#include <cassert>
#include "../src/watermark.h"

void test_client_limit() {
    // 物理速率 = 6Mbps (6000000), 时隙 = 25ms, MTU = 1450
    // 水位线 = ceil((6000000 * 25 * 1.5) / (8 * 1450 * 1000)) = ceil(19.39) = 20
    size_t limit = calculate_dynamic_limit(25, 6000000, 1450);
    assert(limit == 20);
}

int main() {
    test_client_limit();
    return 0;
}
