// src/mac_token.h
#pragma once
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)
// 控制帧结构体，脱离 FEC 与 IP 路由体系
struct TokenFrame {
    uint8_t  magic;       // 魔数，用于快速区分控制帧与数据帧 (如 0x02)
    uint8_t  target_node; // 目标节点编号 (0=广播/ALL)
    uint16_t duration_ms; // 授权发射时间片 (毫秒)
    uint32_t seq_num;     // 全局单调递增序号，防止 RX 缓冲区旧包诈尸
};
#pragma pack(pop)

void serialize_token(const TokenFrame& token, uint8_t* buffer);
TokenFrame parse_token(const uint8_t* buffer);
