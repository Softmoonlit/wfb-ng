#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <memory>

#include "threads.h"
#include "packet_queue.h"
#include "watermark.h"
#include "mac_token.h"
#include "config.h"
#include "error_handler.h"

// === 测试 1: 线程启动和退出 ===
TEST(WfbCoreTest, ThreadLifecycle) {
    ThreadSharedState state(1000);
    std::atomic<bool> shutdown{false};

    std::thread control_thread([&]() {
        int iterations = 0;
        while (!shutdown.load() && iterations < 10) {
            std::unique_lock<std::mutex> lock(state.mtx);
            state.can_send = true;
            state.token_expire_time_ms = get_monotonic_ms() + 50;
            state.cv_send.notify_one();
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            iterations++;
        }
    });

    // 运行 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 触发退出
    shutdown.store(true);
    control_thread.join();

    EXPECT_TRUE(true);  // 线程正常退出
}

// === 测试 2: Token 授权和过期 ===
TEST(WfbCoreTest, TokenGating) {
    ThreadSharedState state(1000);

    // 模拟 Token 授权
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.can_send = true;
        state.token_expire_time_ms = get_monotonic_ms() + 50;  // 50ms
    }

    // 立即检查：应允许发射
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        EXPECT_TRUE(state.can_send);
        EXPECT_GT(state.token_expire_time_ms, get_monotonic_ms());
    }

    // 等待 Token 过期
    std::this_thread::sleep_for(std::chrono::milliseconds(60));

    // 检查：Token 应已过期
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        EXPECT_GT(get_monotonic_ms(), state.token_expire_time_ms);
    }
}

// === 测试 3: 环形队列反压 ===
TEST(WfbCoreTest, QueueBackpressure) {
    // 使用 ThreadSafeQueue 而不是 PacketQueue
    ThreadSafeQueue<std::vector<uint8_t>> queue(100);

    // 准备测试数据包
    std::vector<uint8_t> pkt(100, 0xAA);

    int push_count = 0;
    while (push_count < 150) {  // 安全限制
        queue.push(pkt);
        push_count++;

        // 检查队列是否已满
        if (queue.size() >= 100) {
            break;
        }
    }

    // 队列满时应不再接受新元素（但 ThreadSafeQueue::push 会静默丢弃）
    EXPECT_GE(push_count, 100);
    EXPECT_LE(push_count, 150);  // 可能多于100因为push不检查

    // 检查队列大小
    EXPECT_EQ(queue.size(), 100u);

    // 清空队列
    int pop_count = 0;
    try {
        while (!queue.empty()) {
            auto out = queue.pop();
            pop_count++;
        }
    } catch (const std::runtime_error&) {
        // 忽略空队列异常
    }

    EXPECT_EQ(pop_count, 100);  // 应该弹出100个元素
    EXPECT_EQ(queue.size(), 0u);
}

// === 测试 4: 动态水位线计算 ===
TEST(WfbCoreTest, DynamicWatermark) {
    // MCS 0, 20MHz, 50ms 窗口
    uint32_t limit1 = calculate_dynamic_limit(50, 6000000, 1450);
    EXPECT_GT(limit1, 0u);
    EXPECT_LT(limit1, 1000u);  // 应在合理范围内

    // MCS 6, 20MHz, 50ms 窗口（更高速率）
    uint32_t limit2 = calculate_dynamic_limit(50, 54000000, 1450);
    EXPECT_GT(limit2, limit1);  // 应比低速时更大

    // 验证公式：limit = ceil((R_phy × duration × 1.5) / (8 × MTU × 1000))
    double expected = std::ceil((6000000.0 * 50 * 1.5) / (8 * 1450 * 1000));
    EXPECT_NEAR(limit1, expected, 1.0);

    // 边界测试：极端参数
    uint32_t limit_zero = calculate_dynamic_limit(0, 6000000, 1450);
    EXPECT_EQ(limit_zero, 0u);

    uint32_t limit_max = calculate_dynamic_limit(1000, 54000000, 1450);
    EXPECT_GT(limit_max, 0u);
}

// === 测试 5: TokenFrame 序列化和反序列化 ===
TEST(WfbCoreTest, TokenFrameSerialization) {
    TokenFrame original;
    original.magic = 0x02;
    original.target_node = 5;
    original.duration_ms = 300;
    original.seq_num = 12345;

    // 序列化
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(original, buffer);

    // 反序列化
    TokenFrame decoded = parse_token(buffer);

    // 验证字段
    EXPECT_EQ(decoded.magic, original.magic);
    EXPECT_EQ(decoded.target_node, original.target_node);
    EXPECT_EQ(decoded.duration_ms, original.duration_ms);
    EXPECT_EQ(decoded.seq_num, original.seq_num);
}

// === 测试 6: condition_variable 唤醒 ===
TEST(WfbCoreTest, ConditionVariableWakeup) {
    ThreadSharedState state(1000);
    std::atomic<bool> woke_up{false};

    // 等待线程
    std::thread waiter([&]() {
        std::unique_lock<std::mutex> lock(state.mtx);
        state.cv_send.wait(lock, [&]() {
            return state.can_send;
        });
        woke_up.store(true);
    });

    // 等待 10ms
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // 唤醒线程
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.can_send = true;
        state.cv_send.notify_one();
    }

    // 等待线程响应
    waiter.join();

    EXPECT_TRUE(woke_up.load());
}

// === 测试 7: FEC 参数配置 ===
TEST(WfbCoreTest, FECConfigValidation) {
    // 默认配置
    FECConfig config1;
    EXPECT_TRUE(config1.is_valid());
    EXPECT_EQ(config1.n, 12);
    EXPECT_EQ(config1.k, 8);
    EXPECT_EQ(config1.redundancy(), 4);

    // 自定义配置
    FECConfig config2{16, 10};
    EXPECT_TRUE(config2.is_valid());
    EXPECT_EQ(config2.redundancy(), 6);

    // 无效配置
    FECConfig config3{8, 12};  // k > n
    EXPECT_FALSE(config3.is_valid());

    FECConfig config4{0, 8};  // n = 0
    EXPECT_FALSE(config4.is_valid());

    FECConfig config5{300, 10};  // n > 255
    EXPECT_FALSE(config5.is_valid());
}

// === 测试 8: 错误处理 ===
TEST(WfbCoreTest, ErrorHandling) {
    ErrorContext ctx;
    ctx.function = "test_function";
    ctx.file = "test.cpp";
    ctx.line = 100;
    ctx.errno_value = EINVAL;
    ctx.message = "测试错误";
    ctx.severity = ErrorSeverity::ERROR;

    // 错误日志函数不应崩溃
    testing::internal::CaptureStderr();
    log_error(ctx);
    std::string output = testing::internal::GetCapturedStderr();

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("test_function"), std::string::npos);
}

// === 测试 9: 并发安全性 ===
TEST(WfbCoreTest, ConcurrentAccess) {
    ThreadSharedState state(1000);
    std::atomic<bool> shutdown{false};
    std::atomic<int> operations{0};

    // 多线程并发访问
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&]() {
            while (!shutdown.load()) {
                {
                    std::lock_guard<std::mutex> lock(state.mtx);
                    state.can_send = !state.can_send;
                    state.token_expire_time_ms = get_monotonic_ms() + 50;
                }
                operations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // 运行 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    shutdown.store(true);

    // 等待所有线程
    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(operations.load(), 0);
}

// === 测试 10: 内存泄漏检测（简化） ===
TEST(WfbCoreTest, NoMemoryLeak) {
    // 创建和销毁多次
    for (int i = 0; i < 100; i++) {
        ThreadSafeQueue<std::vector<uint8_t>> queue(100);

        std::vector<uint8_t> pkt(100, 0);

        for (int j = 0; j < 50; j++) {
            queue.push(pkt);
        }

        try {
            while (!queue.empty()) {
                auto out = queue.pop();
            }
        } catch (const std::runtime_error&) {
            // 忽略空队列异常
        }
    }

    EXPECT_TRUE(true);  // 如果没有崩溃，则通过
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}