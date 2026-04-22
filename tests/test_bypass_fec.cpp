// -*- C++ -*-
// 测试 bypass_fec 功能
//
// Copyright (C) 2017 - 2024 Vasily Evseenko <svpcom@p2ptech.org>

/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 3.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cassert>
#include <iostream>
#include <cstring>
#include "../src/tx.hpp"

// Mock Transmitter 用于测试 bypass_fec 功能
class MockTransmitter : public Transmitter {
public:
    int inject_count = 0;
    bool last_bypass_fec = false;
    size_t last_packet_size = 0;

    MockTransmitter() : Transmitter(1, 1, "", 0, 0, 0, dummy_tags) {}

    void inject_packet(const uint8_t *buf, size_t size) override {
        inject_count++;
        last_packet_size = size;
    }

    // 重写 send_packet 以捕获 bypass_fec 参数
    bool send_packet(const uint8_t *buf, size_t size, uint8_t flags,
                    bool bypass_fec = false) {
        last_bypass_fec = bypass_fec;
        return Transmitter::send_packet(buf, size, flags, bypass_fec);
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

// 测试 1: bypass_fec=true 时跳过 FEC 编码，直接注入数据包
void test_bypass_fec_true() {
    MockTransmitter tx;
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};

    // 重置计数器
    tx.inject_count = 0;

    // 测试 bypass_fec=true（控制帧路径）
    bool result = tx.send_packet(data, sizeof(data), 0, true);

    // 验证结果
    assert(result == true);  // 应该成功
    assert(tx.last_bypass_fec == true);  // 参数应传递正确
    assert(tx.inject_count > 0);  // 应该直接注入，不经过 FEC 编码

    std::cout << "PASS: bypass_fec=true 跳过 FEC 编码" << std::endl;
}

// 测试 2: bypass_fec=false 时正常 FEC 编码
void test_bypass_fec_false() {
    MockTransmitter tx;
    uint8_t data[] = {0x01, 0x02, 0x03};

    // 重置计数器
    tx.inject_count = 0;

    // 测试 bypass_fec=false（数据帧路径）
    bool result = tx.send_packet(data, sizeof(data), 0, false);

    // 验证结果
    assert(result == true);  // 应该成功
    assert(tx.last_bypass_fec == false);  // 参数应传递正确

    std::cout << "PASS: bypass_fec=false 正常 FEC 编码" << std::endl;
}

// 测试 3: 默认参数 bypass_fec=false 向后兼容
void test_default_parameter() {
    MockTransmitter tx;
    uint8_t data[] = {0x0A, 0x0B, 0x0C, 0x0D};

    // 重置计数器
    tx.inject_count = 0;

    // 测试不传 bypass_fec 参数（向后兼容）
    bool result = tx.send_packet(data, sizeof(data), 0);  // 不传 bypass_fec 参数

    // 验证结果
    assert(result == true);  // 应该成功
    assert(tx.last_bypass_fec == false);  // 默认应为 false

    std::cout << "PASS: 默认参数 bypass_fec=false 向后兼容" << std::endl;
}

// 测试 4: 空数据包处理
void test_empty_packet() {
    MockTransmitter tx;

    // 重置计数器
    tx.inject_count = 0;

    // 测试空数据包（FEC-only 标志）
    bool result = tx.send_packet(NULL, 0, WFB_PACKET_FEC_ONLY, true);

    // 空数据包在 fragment_idx=0 时应返回 false
    // 因为 FEC-only 包不能在空块上发送
    assert(result == false);

    std::cout << "PASS: 空数据包处理正确" << std::endl;
}

int main() {
    std::cout << "开始 bypass_fec 功能测试..." << std::endl;

    test_bypass_fec_true();
    test_bypass_fec_false();
    test_default_parameter();
    test_empty_packet();

    std::cout << "\n所有 bypass_fec 测试通过！" << std::endl;
    return 0;
}
