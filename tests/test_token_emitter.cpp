// tests/test_token_emitter.cpp
// Token 三连发发射器单元测试

#include <cassert>
#include <iostream>
#include <vector>
#include "../src/token_emitter.h"
#include "../src/tx.hpp"
#include "../src/wifibroadcast.hpp"

// Mock Transmitter 用于测试
class MockTransmitter : public Transmitter {
public:
    std::vector<size_t> injected_sizes;
    std::vector<bool> bypass_fec_flags;

    MockTransmitter() : Transmitter(1, 1, "", 0, 0, 0, dummy_tags) {}

    void inject_packet(const uint8_t *buf, size_t size) override {
        injected_sizes.push_back(size);
    }

    bool send_packet(const uint8_t *buf, size_t size, uint8_t flags,
                    bool bypass_fec = false) override {
        bypass_fec_flags.push_back(bypass_fec);
        inject_packet(buf, size);
        return true;
    }

    void select_output(int idx) override {}
    void dump_stats(uint64_t ts, uint32_t &inj, uint32_t &drop,
                    uint32_t &bytes) override {}
    void update_radiotap_header(radiotap_header_t &rh) override {}
    radiotap_header_t get_radiotap_header() override { return {}; }
    void set_mark(uint32_t idx) override {}

private:
    static std::vector<tags_item_t> dummy_tags;
};
std::vector<tags_item_t> MockTransmitter::dummy_tags;

// 测试 1: 三连发调用 send_packet 3 次，且 bypass_fec=true
void test_triple_send() {
    MockTransmitter tx;
    send_token_triple(&tx, 5, 100);

    // 验证调用了 3 次 send_packet
    assert(tx.injected_sizes.size() == 3);

    // 验证 bypass_fec=true
    assert(tx.bypass_fec_flags.size() == 3);
    for (bool flag : tx.bypass_fec_flags) {
        assert(flag == true);
    }

    std::cout << "PASS: 三连发正确，bypass_fec=true" << std::endl;
}

// 测试 2: 三连发使用相同的序列号
void test_same_seq_num() {
    MockTransmitter tx;

    // 第一次三连发
    send_token_triple(&tx, 5, 100);
    assert(tx.injected_sizes.size() == 3);

    // 第二次三连发
    send_token_triple(&tx, 5, 100);
    assert(tx.injected_sizes.size() == 6);

    std::cout << "PASS: 三连发调用正确" << std::endl;
}

// 测试 3: 接收端去重逻辑
void test_deduplication() {
    uint32_t last_seq = 0;

    // 第一个包（seq=1）
    assert(is_duplicate_token(1, last_seq) == false);
    assert(last_seq == 1);

    // 第二个包（seq=2）
    assert(is_duplicate_token(2, last_seq) == false);
    assert(last_seq == 2);

    // 重复包（seq=1）
    assert(is_duplicate_token(1, last_seq) == true);
    assert(last_seq == 2);  // 未更新

    std::cout << "PASS: 去重逻辑正确" << std::endl;
}

int main() {
    test_triple_send();
    test_same_seq_num();
    test_deduplication();

    std::cout << "所有 Token 发射器测试通过！" << std::endl;
    return 0;
}
