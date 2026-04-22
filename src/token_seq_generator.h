// src/token_seq_generator.h
// 全局单调递增序列号生成器

#pragma once
#include <atomic>
#include <cstdint>

// 全局序列号生成器，线程安全，从 0 开始
// 用于 Token 帧序列号，防止 RX 缓冲区旧包诈尸
extern std::atomic<uint32_t> g_token_seq_num;

// 获取下一个序列号（原子递增）
// 返回：递增后的新序列号
uint32_t get_next_token_seq();

// 获取当前序列号（不递增）
// 返回：当前序列号
uint32_t get_current_token_seq();
