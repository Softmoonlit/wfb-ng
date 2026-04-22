// src/server_scheduler.cpp
#include "server_scheduler.h"
#include "aq_sq_manager.h"
#include "guard_interval.h"
#include "threads.h"  // for kGuardIntervalMs

void ServerScheduler::init_node(uint8_t node_id) {
    nodes[node_id] = NodeState{};
}

void ServerScheduler::set_aq_sq_manager(AqSqManager* manager) {
    aq_sq_manager = manager;
}

void ServerScheduler::set_num_nodes(size_t n) {
    num_nodes = n;
}

uint16_t ServerScheduler::get_max_window() const {
    // 动态计算 MAX_WINDOW（per SCHED-06）
    // 确保 (N-1) × MAX_WINDOW < UFTP GRTT（假设 5000ms）
    if (num_nodes <= 5) return 600;   // 5 节点及以下
    if (num_nodes <= 10) return 300;  // 10 节点及以下
    return 150;                        // 更多节点进一步减小
}

uint16_t ServerScheduler::calculate_next_window(uint8_t node_id, uint16_t actual_used, uint16_t allocated) {
    auto& state = nodes[node_id];
    uint16_t max_window = get_max_window();

    // 状态 I：极速枯竭与睡眠 (Zero-Payload Drop, < 5ms)
    // Watchdog 保护：连续 3 次静默才判定断流（per SCHED-02）
    if (actual_used < 5) {
        state.silence_count++;
        if (state.silence_count >= 3) {
            // 连续 3 次静默，判定断流
            state.current_state = NodeStateEnum::STATE_I;
            state.current_window = MIN_WINDOW;
            state.in_aq = false;
            // 迁移到 SQ（睡眠队列）
            if (aq_sq_manager) {
                aq_sq_manager->migrate_to_sq(node_id);
            }
            return MIN_WINDOW;
        }
        // Watchdog 保护：保持原窗口，不立即降级
        return allocated;
    }

    // 有活动，重置静默计数
    state.silence_count = 0;

    // 状态 III：高负载贪突发 (Additive Increase, t >= allocated)
    // 使用公式 min(MAX, allocated + STEP)（per SCHED-04）
    if (actual_used >= allocated) {
        state.current_state = NodeStateEnum::STATE_III;
        uint16_t target = allocated + STEP_UP;
        state.current_window = std::min(target, max_window);
        state.in_aq = true;
        // 确保在 AQ（活跃队列）中
        if (aq_sq_manager) {
            aq_sq_manager->migrate_to_aq(node_id);
        }
        return state.current_window;
    }

    // 状态 II：需量拟合 (Demand-Fitting, 5ms <= t < allocated)
    // 使用公式 max(MIN, actual + MARGIN)（per SCHED-03）
    state.current_state = NodeStateEnum::STATE_II;
    uint16_t target = actual_used + ELASTIC_MARGIN;
    state.current_window = std::max(target, MIN_WINDOW);
    state.in_aq = true;
    // 迁移到 AQ（活跃队列）
    if (aq_sq_manager) {
        aq_sq_manager->migrate_to_aq(node_id);
    }
    return state.current_window;
}

std::pair<uint8_t, uint16_t> ServerScheduler::get_next_node_to_serve() {
    if (!aq_sq_manager) {
        return {0xFF, 0};  // 无效节点
    }

    // 获取下一个节点（混合交织调度）
    auto [node_id, is_from_sq] = aq_sq_manager->get_next_node(INTERLEAVE_RATIO);

    if (node_id == 0xFF) {
        return {0xFF, 0};  // 无节点可服务
    }

    uint16_t duration_ms;
    if (is_from_sq) {
        // SQ 节点：使用微探询时长（per SCHED-09）
        duration_ms = MICRO_PROBE_DURATION_MS;
    } else {
        // AQ 节点：使用当前授权窗口
        auto it = nodes.find(node_id);
        if (it != nodes.end()) {
            duration_ms = it->second.current_window;
        } else {
            duration_ms = MIN_WINDOW;
        }
    }

    return {node_id, duration_ms};
}

void ServerScheduler::execute_scheduling_cycle() {
    // 1. 获取下一个要服务的节点
    auto [node_id, duration_ms] = get_next_node_to_serve();

    if (node_id == 0xFF) {
        return;  // 无节点
    }

    // 2. 发射 Token（需要集成 TokenEmitter）
    // token_emitter->send_token_triple(transmitter, node_id, duration_ms);

    // 3. 插入保护间隔（per SCHED-13, 使用 timerfd + 短自旋）
    apply_guard_interval(kGuardIntervalMs);
}

void ServerScheduler::update_node_state(uint8_t node_id, uint16_t actual_used, uint16_t allocated) {
    // 调用 calculate_next_window 更新状态
    uint16_t next_window = calculate_next_window(node_id, actual_used, allocated);

    // 日志记录（可选）
    // printf("Node %d: actual=%d, allocated=%d, next=%d\n",
    //        node_id, actual_used, allocated, next_window);
}
