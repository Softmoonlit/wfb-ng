// tests/test_mac_token.cpp
#include <cassert>
#include "../src/mac_token.h"

void test_token_serialization() {
    TokenFrame original{0x02, 5, 25, 1001};
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(original, buffer);
    TokenFrame parsed = parse_token(buffer);

    // 验证序列化和反序列化是否正确
    assert(parsed.magic == 0x02);
    assert(parsed.target_node == 5);
    assert(parsed.duration_ms == 25);
    assert(parsed.seq_num == 1001);
}

int main() {
    test_token_serialization();
    return 0;
}
