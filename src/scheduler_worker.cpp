#include "scheduler_worker.h"
#include "server_scheduler.h"
#include "mac_token.h"
#include "threads.h"  // 需要 get_monotonic_ms()

// 前向声明
class AqSqManager;
class TokenEmitterAdapter;

#include <iostream>
#include <algorithm>
#include <chrono>
#include <unistd.h>

// TokenEmitter 存根类（供编译测试）
class TokenEmitterAdapter {
public:
    TokenEmitterAdapter(pcap_t* pcap_handle) {
        // 存根实现
    }

    void set_transmitter(void* transmitter) {
        // 存根实现
    }

    bool send_token_triple(const TokenFrame& token) {
        // 存根实现，总是返回成功
        std::cout << "Token 发射: 节点=" << static_cast<int>(token.target_node)
                  << ", 时长=" << token.duration_ms << "ms" << std::endl;
        return true;
    }

    uint32_t get_next_seq() {
        static uint32_t seq = 0;
        return ++seq;
    }
};

// 辅助函数：应用保护间隔（存根实现）
void apply_guard_interval(uint32_t guard_interval_ms) {
    // 存根实现，记录日志
    std::cout << "应用保护间隔: " << guard_interval_ms << "ms" << std::endl;
    usleep(guard_interval_ms * 1000);
}

// 辅助函数：检查是否所有节点都处于空闲状态（存根实现）
bool all_nodes_in_idle_state() {
    // 存根实现，总是返回 false 以便测试主循环
    return false;
}

SchedulerError scheduler_main_loop(
    ThreadSharedState* shared_state,
    ServerScheduler* scheduler,
    pcap_t* pcap_handle,
    const SchedulerConfig& config,
    std::atomic<bool>* shutdown,
    SchedulerStats* stats
) {
    // 参数验证
    if (!shared_state || !scheduler || !pcap_handle || !shutdown) {
        return SchedulerError::UNKNOWN_ERROR;
    }

    // 初始化呼吸周期（下行广播）
    // BreathingCycle breathing_cycle;  // 暂时注释，避免编译错误

    // Token 发射器适配器
    TokenEmitterAdapter token_emitter(pcap_handle);

    std::cout << "调度器线程启动，节点数=" << static_cast<int>(config.node_count)
              << ", 优先级=95" << std::endl;

    // 主调度循环
    while (!shutdown->load()) {
        // 1. 检查是否所有节点都处于空闲状态
        if (all_nodes_in_idle_state()) {
            // 全员深睡，执行空闲巡逻逻辑
            if (stats) stats->idle_patrols++;

            // 长间隔空闲巡逻（100ms）
            usleep(100000);
            continue;
        }

        // 2. 获取下一个要服务的节点（存根实现）
        // TODO: 调用 scheduler->get_next_node_to_serve()
        uint8_t node_id = 1;  // 模拟节点 ID
        uint32_t time_quota_ms = 100;  // 模拟授权时间

        if (node_id == 0xFF) {
            // 没有节点可服务，执行空闲巡逻
            if (stats) stats->idle_patrols++;
            usleep(100000);
            continue;
        }

        // 限制在配置范围内
        time_quota_ms = std::max(config.min_window_ms,
                                 std::min(config.max_window_ms, time_quota_ms));

        // 3. 构建 TokenFrame
        TokenFrame token;
        token.magic = 0x02;
        token.target_node = node_id;
        token.duration_ms = static_cast<uint16_t>(time_quota_ms);  // 注意类型转换
        token.seq_num = token_emitter.get_next_seq();

        // 4. 三连发 Token（击穿机制）
        bool send_success = token_emitter.send_token_triple(token);
        if (!send_success) {
            std::cerr << "Token 发射失败，节点=" << static_cast<int>(node_id) << std::endl;
            if (stats) stats->tokens_failed++;

            // 发射失败，等待一段时间后重试下一个节点
            usleep(10000);  // 10ms
            continue;
        }

        if (stats) stats->tokens_sent++;

        // 5. 应用保护间隔（节点切换）
        apply_guard_interval(config.guard_interval_ms);
        if (stats) stats->node_switches++;

        // 6. 更新共享状态（供发射线程使用）
        {
            std::lock_guard<std::mutex> lock(shared_state->mtx);
            shared_state->can_send = true;
            shared_state->token_expire_time_ms =
                get_monotonic_ms() + time_quota_ms;
            shared_state->cv_send.notify_one();
        }

        // 7. 等待授权窗口过期（带 shutdown 检查）
        uint64_t start_ms = get_monotonic_ms();
        while (!shutdown->load()) {
            uint64_t elapsed = get_monotonic_ms() - start_ms;
            if (elapsed >= time_quota_ms) break;

            // 短间隔检查（10ms）
            usleep(10000);
        }

        if (shutdown->load()) break;

        // 8. 更新节点状态
        // TODO: 调用 scheduler->update_node_state()
        // uint16_t allocated_ms = static_cast<uint16_t>(time_quota_ms);
        // uint16_t actual_used_ms = allocated_ms;
        // scheduler->update_node_state(node_id, actual_used_ms, allocated_ms);

        // 9. 呼吸周期管理（仅服务端）
        // TODO: 调用 breathing_cycle.advance_phase() 和 is_inhale_phase()
        // breathing_cycle.advance_phase();
        // if (breathing_cycle.is_inhale_phase()) {
        //     std::cout << "吸气阶段：轮询节点 NACK" << std::endl;
        // }
    }

    std::cout << "调度器线程正常退出" << std::endl;
    return SchedulerError::OK;
}