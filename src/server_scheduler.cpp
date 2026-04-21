// src/server_scheduler.cpp
#include "server_scheduler.h"

void ServerScheduler::init_node(uint8_t node_id) {
    nodes[node_id] = NodeState{};
}

uint16_t ServerScheduler::calculate_next_window(uint8_t node_id, uint16_t actual_used, uint16_t allocated) {
    auto& state = nodes[node_id];

    // 状态 I：极速枯竭与睡眠 (Zero-Payload Drop)
    if (actual_used < 5) {
        state.silence_count++;
        if (state.silence_count >= 3) {
            state.in_aq = false;
            return MIN_WINDOW; // 连续3次静默，断流退避
        }
        return allocated; // Watchdog 保护：保持原有时隙
    }

    state.silence_count = 0;
    state.in_aq = true;

    // 状态 III：高负载贪突发 (Additive Increase)
    if (actual_used >= allocated) {
        uint16_t target = allocated + STEP_UP;
        return std::min(target, MAX_WINDOW);
    }
    // 状态 II：需量拟合与退避 (Demand-Fitting)
    else {
        uint16_t target = actual_used + ELASTIC_MARGIN;
        return std::max(target, MIN_WINDOW);
    }
}
