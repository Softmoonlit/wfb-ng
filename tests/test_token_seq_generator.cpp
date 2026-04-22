// tests/test_token_seq_generator.cpp
// 序列号生成器单元测试

#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <set>
#include "../src/token_seq_generator.h"

// 测试 1: 初始序列号为 0
void test_initial_seq() {
    // 注意：由于是全局序列号，此测试仅在序列号生成器首次使用时有效
    // 在单元测试中，我们重置序列号为 0 进行测试
    // 但 std::atomic 没有提供重置功能，所以这里我们只验证序列号递增

    uint32_t current = get_current_token_seq();

    // 验证序列号递增
    uint32_t next = get_next_token_seq();
    assert(next == current + 1 || (current == 0 && next == 1));

    std::cout << "PASS: 序列号初始测试" << std::endl;
}

// 测试 2: 序列号严格递增
void test_seq_increment() {
    // 验证序列号递增
    uint32_t seq1 = get_next_token_seq();
    uint32_t seq2 = get_next_token_seq();
    uint32_t seq3 = get_next_token_seq();

    assert(seq2 == seq1 + 1);
    assert(seq3 == seq2 + 1);

    std::cout << "PASS: 序列号严格递增" << std::endl;
}

// 测试 3: 多线程并发调用无竞态条件
void test_thread_safety() {
    // 多线程并发测试
    const int num_threads = 10;
    const int ops_per_thread = 1000;
    std::vector<std::thread> threads;
    std::vector<std::set<uint32_t>> results(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                uint32_t seq = get_next_token_seq();
                results[i].insert(seq);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // 验证所有序列号唯一（无重复）
    std::set<uint32_t> all_seqs;
    for (const auto& s : results) {
        all_seqs.insert(s.begin(), s.end());
    }

    // 验证序列号不重复
    assert(all_seqs.size() == static_cast<size_t>(num_threads * ops_per_thread));

    std::cout << "PASS: 多线程并发无竞态条件" << std::endl;
}

int main() {
    std::cout << "开始序列号生成器测试..." << std::endl;

    test_initial_seq();
    test_seq_increment();
    test_thread_safety();

    std::cout << "所有序列号生成器测试通过！" << std::endl;
    return 0;
}
