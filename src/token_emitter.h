// src/token_emitter.h
// Token 三连发发射器

#pragma once
#include "mac_token.h"
#include "tx.hpp"

// Token 三连发发射器
// 参数:
//   transmitter: 发射器实例
//   target_node: 目标节点编号
//   duration_ms: 授权发射时间片（毫秒）
void send_token_triple(Transmitter* transmitter,
                       uint8_t target_node,
                       uint16_t duration_ms);

// 接收端序列号去重
// 参数:
//   seq_num: 接收到的序列号
//   last_seq: 上一次接收的序列号（引用，会被更新）
// 返回: true 表示重复，false 表示新包
bool is_duplicate_token(uint32_t seq_num, uint32_t& last_seq);
