#include "scheduler_worker.h"
#include "server_scheduler.h"
#include "mac_token.h"
#include "threads.h"  // 需要 get_monotonic_ms()
#include "guard_interval.h"  // Gap-03d: 保护间隔

// 前向声明
class AqSqManager;

#include <iostream>
#include <algorithm>
#include <chrono>
#include <unistd.h>
#include <atomic>

// 全局序列号生成器（线程安全）
static std::atomic<uint32_t> g_token_seq{0};

// Token 发射器（简化实现）
class TokenEmitter {
public:
    TokenEmitter(pcap_t* pcap_handle) : pcap_handle_(pcap_handle) {}

    // 三连发 Token（Gap-03b: 击穿机制）
    bool send_token_triple(const TokenFrame& token) {
        if (!pcap_handle_) {
            std::cerr << "pcap 句柄无效" << std::endl;
            return false;
        }

        // 序列化 Token（使用自由函数）
        uint8_t buffer[64];
        serialize_token(token, buffer);
        size_t len = sizeof(TokenFrame);  // TokenFrame 固定长度

        // 三连发（间隔 1ms）
        for (int i = 0; i < 3; i++) {
            int ret = pcap_inject(pcap_handle_, buffer, len);
            if (ret < 0) {
                std::cerr << "Token 发射失败 (尝试 " << i + 1 << "/3): "
                          << pcap_geterr(pcap_handle_) << std::endl;
                // 重试
                usleep(1000);
                continue;
            }

            // 打印日志（仅第一次）
            if (i == 0) {
                std::cout << "Token 三连发: 节点=" << static_cast<int>(token.target_node)
                          << ", 时长=" << token.duration_ms << "ms"
                          << ", 序列号=" << token.seq_num << std::endl;
            }

            // 三连发间隔
            if (i < 2) usleep(1000);
        }

        return true;
    }

    uint32_t get_next_seq() {
        return ++g_token_seq;
    }

private:
    pcap_t* pcap_handle_;
};

SchedulerError scheduler_main_loop(
    ThreadSharedState* shared_state,
    ServerScheduler* scheduler,
    pcap_t* pcap_handle,
    const SchedulerConfig& config,
    std::atomic<bool>* shutdown,
    SchedulerStats* stats
) {
    // 参数验证
    if (!shared_state || !scheduler || !shutdown) {
        return SchedulerError::UNKNOWN_ERROR;
    }

    // Token 发射器
    TokenEmitter token_emitter(pcap_handle);

    // 上次服务的节点 ID（用于节点切换检测）
    uint8_t last_node_id = 0xFF;

    std::cout << "调度器线程启动，节点数=" << static_cast<int>(config.node_count)
              << ", 优先级=95" << std::endl;

    // 主调度循环
    while (!shutdown->load()) {
        // Gap-03a: 获取下一个要服务的节点
        auto next = scheduler->get_next_node_to_serve();
        uint8_t node_id = next.first;
        uint16_t time_quota_ms = next.second;

        if (node_id == 0xFF) {
            // 没有节点可服务，执行空闲巡逻
            if (stats) stats->idle_patrols++;
            usleep(100000);  // 100ms 空闲巡逻间隔
            continue;
        }

        // Gap-03d: 节点切换时应用保护间隔
        if (last_node_id != 0xFF && last_node_id != node_id) {
            apply_guard_interval(kGuardIntervalMs);
            if (stats) stats->node_switches++;
        }
        last_node_id = node_id;

        // 限制在配置范围内（Gap-09: 类型安全）
        uint32_t capped_quota = std::max(static_cast<uint32_t>(config.min_window_ms),
                                         std::min(static_cast<uint32_t>(config.max_window_ms),
                                                  static_cast<uint32_t>(time_quota_ms)));

        // 检查类型转换安全
        if (capped_quota > 65535) {
            std::cerr << "警告: 时间配额溢出，截断为 65535ms" << std::endl;
            capped_quota = 65535;
        }

        // 构建 TokenFrame
        TokenFrame token;
        token.magic = 0x02;
        token.target_node = node_id;
        token.duration_ms = static_cast<uint16_t>(capped_quota);
        token.seq_num = token_emitter.get_next_seq();

        // Gap-03b: 三连发 Token（击穿机制）
        bool send_success = token_emitter.send_token_triple(token);
        if (!send_success) {
            std::cerr << "Token 发射失败，节点=" << static_cast<int>(node_id) << std::endl;
            if (stats) stats->tokens_failed++;
            usleep(10000);  // 10ms 后重试
            continue;
        }

        if (stats) stats->tokens_sent++;

        // 更新共享状态（供发射线程使用）
        {
            std::lock_guard<std::mutex> lock(shared_state->mtx);
            shared_state->can_send = true;
            shared_state->token_expire_time_ms = get_monotonic_ms() + capped_quota;
            shared_state->cv_send.notify_one();
        }

        // 等待授权窗口过期（带 shutdown 检查）
        uint64_t start_ms = get_monotonic_ms();
        while (!shutdown->load()) {
            uint64_t elapsed = get_monotonic_ms() - start_ms;
            if (elapsed >= capped_quota) break;
            usleep(10000);  // 10ms 检查间隔
        }

        if (shutdown->load()) break;

        // Gap-03c: 更新节点状态
        uint16_t allocated_ms = static_cast<uint16_t>(capped_quota);
        uint16_t actual_used_ms = allocated_ms;  // 简化：假设完全使用
        scheduler->update_node_state(node_id, actual_used_ms, allocated_ms);
    }

    std::cout << "调度器线程正常退出" << std::endl;
    return SchedulerError::OK;
}