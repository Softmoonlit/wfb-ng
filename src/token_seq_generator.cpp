// src/token_seq_generator.cpp
// 全局单调递增序列号生成器实现

#include "token_seq_generator.h"

// 全局序列号，从 0 开始初始化
// 使用 std::atomic 确保线程安全
std::atomic<uint32_t> g_token_seq_num{0};

// 获取下一个序列号（原子递增）
// 使用 fetch_add(1) 实现原子递增
// fetch_add(1) 返回旧值，加 1 得到新值
uint32_t get_next_token_seq() {
    return g_token_seq_num.fetch_add(1) + 1;
}

// 获取当前序列号（原子读取，不递增）
// 使用 load() 原子读取当前值
uint32_t get_current_token_seq() {
    return g_token_seq_num.load();
}
