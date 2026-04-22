// tests/test_mac_token.cpp
// TokenFrame 单元测试：验证控制帧结构体的字节对齐和序列化/反序列化正确性

#include <cassert>
#include <iostream>
#include <cstring>
#include <cstddef>  // 用于 offsetof 宏
#include "../src/mac_token.h"

// 测试 1: 验证 TokenFrame 字节对齐
void test_token_frame_size() {
    // 验证 TokenFrame 结构体大小为 8 字节（无填充）
    // 字段布局：
    //   uint8_t  magic       (1 字节)
    //   uint8_t  target_node (1 字节)
    //   uint16_t duration_ms (2 字节)
    //   uint32_t seq_num     (4 字节)
    //   总计：1 + 1 + 2 + 4 = 8 字节

    assert(sizeof(TokenFrame) == 8);

    std::cout << "PASS: TokenFrame 大小为 8 字节（无填充）" << std::endl;
}

void test_token_frame_alignment() {
    // 验证字段偏移量正确
    TokenFrame token;

    // 验证字段偏移量（per ROADMAP 成功标准 1）
    assert(offsetof(TokenFrame, magic) == 0);
    assert(offsetof(TokenFrame, target_node) == 1);
    assert(offsetof(TokenFrame, duration_ms) == 2);
    assert(offsetof(TokenFrame, seq_num) == 4);

    std::cout << "PASS: TokenFrame 字段对齐正确" << std::endl;
}

// 测试 2: 验证序列化和反序列化函数
void test_serialize_token() {
    // 创建测试 TokenFrame
    TokenFrame token;
    token.magic = 0x02;
    token.target_node = 5;
    token.duration_ms = 100;
    token.seq_num = 12345;

    // 序列化
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(token, buffer);

    // 验证字节序（小端序）
    assert(buffer[0] == 0x02);                       // magic
    assert(buffer[1] == 5);                          // target_node
    assert(buffer[2] == 100 && buffer[3] == 0);      // duration_ms (小端序)
    // seq_num 字段验证（小端序）
    // 12345 = 0x3039，小端序为 0x39 0x30 0x00 0x00
    assert(buffer[4] == 0x39 && buffer[5] == 0x30 &&
           buffer[6] == 0x00 && buffer[7] == 0x00);

    std::cout << "PASS: 序列化函数正确编码 TokenFrame" << std::endl;
}

void test_parse_token() {
    // 创建测试缓冲区（小端序）
    uint8_t buffer[sizeof(TokenFrame)] = {0x02, 5, 100, 0, 0x39, 0x30, 0x00, 0x00};

    // 反序列化
    TokenFrame token = parse_token(buffer);

    // 验证字段值
    assert(token.magic == 0x02);
    assert(token.target_node == 5);
    assert(token.duration_ms == 100);
    assert(token.seq_num == 12345);

    std::cout << "PASS: 反序列化函数正确解码 TokenFrame" << std::endl;
}

void test_round_trip() {
    // 测试往返转换：序列化 -> 反序列化（per ROADMAP 成功标准 2）
    TokenFrame original;
    original.magic = 0x02;
    original.target_node = 10;
    original.duration_ms = 200;
    original.seq_num = 67890;

    // 序列化
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(original, buffer);

    // 反序列化
    TokenFrame restored = parse_token(buffer);

    // 验证往返转换正确
    assert(restored.magic == original.magic);
    assert(restored.target_node == original.target_node);
    assert(restored.duration_ms == original.duration_ms);
    assert(restored.seq_num == original.seq_num);

    std::cout << "PASS: 序列化和反序列化可以往返转换" << std::endl;
}

// 测试 3: 运行完整测试套件
int main() {
    std::cout << "=== TokenFrame 单元测试 ===" << std::endl;

    test_token_frame_size();
    test_token_frame_alignment();
    test_serialize_token();
    test_parse_token();
    test_round_trip();

    std::cout << std::endl;
    std::cout << "所有 TokenFrame 测试通过！" << std::endl;
    return 0;
}
