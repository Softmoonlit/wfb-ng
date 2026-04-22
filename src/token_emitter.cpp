// src/token_emitter.cpp
// Token 三连发发射器实现

#include "token_emitter.h"
#include "token_seq_generator.h"
#include "radiotap_template.h"
#include <cstring>

void send_token_triple(Transmitter* transmitter,
                       uint8_t target_node,
                       uint16_t duration_ms) {
    // 生成全局单调递增的序列号
    uint32_t seq_num = get_next_token_seq();

    // 构建 TokenFrame
    TokenFrame token;
    token.magic = 0x02;            // 控制帧魔数
    token.target_node = target_node;
    token.duration_ms = duration_ms;
    token.seq_num = seq_num;

    // 序列化
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(token, buffer);

    // 三连发击穿机制（per D-11, D-12, D-13）
    // 三次发送使用相同的序列号，接收端自动去重
    // 三次发送立即连续，无延迟间隔
    for (int i = 0; i < 3; ++i) {
        transmitter->send_packet(buffer, sizeof(TokenFrame),
                                /*flags=*/0, /*bypass_fec=*/true);
    }
}

bool is_duplicate_token(uint32_t seq_num, uint32_t& last_seq) {
    // 接收端去重逻辑（per D-17, D-18）
    // 新包的 seq_num <= last_seq 时丢弃
    if (seq_num <= last_seq) {
        return true;  // 重复包
    }
    last_seq = seq_num;  // 更新最后接收的序列号
    return false;  // 新包
}
