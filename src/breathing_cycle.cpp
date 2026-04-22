#include "breathing_cycle.h"
#include "guard_interval.h"

// 更新呼吸周期状态，返回当前相位（per SCHED-12）
// 根据时间流逝自动切换相位：
// - EXHALE 持续 800ms 后切换到 INHALE
// - INHALE 持续 200ms 后切换到 EXHALE
BreathingPhase BreathingCycle::update(uint64_t current_time_ms) {
    // 初始化：首次调用记录开始时间
    if (!initialized) {
        phase_start_ms = current_time_ms;
        initialized = true;
        return current_phase;
    }

    uint64_t elapsed = current_time_ms - phase_start_ms;

    if (current_phase == BreathingPhase::EXHALE && elapsed >= EXHALE_DURATION_MS) {
        // 呼出结束，切换到吸气
        current_phase = BreathingPhase::INHALE;
        phase_start_ms = current_time_ms;
        return BreathingPhase::INHALE;
    }
    else if (current_phase == BreathingPhase::INHALE && elapsed >= INHALE_DURATION_MS) {
        // 吸气结束，切换到呼出
        current_phase = BreathingPhase::EXHALE;
        phase_start_ms = current_time_ms;
        return BreathingPhase::EXHALE;
    }

    return current_phase;
}

// 执行吸气阶段：为每个节点分配 NACK 窗口（per SCHED-12）
// 在吸气阶段，服务器暂停下行数据发送，
// 依次为每个节点分配 NACK 回传窗口。
// 每个节点窗口之间插入保护间隔，避免空中撞车。
void BreathingCycle::execute_inhale_phase() {
    // 吸气阶段逻辑框架：
    // 1. 暂停下行数据发送
    // 2. 依次为每个节点分配 NACK 窗口

    for (size_t i = 0; i < num_nodes; i++) {
        uint8_t node_id = static_cast<uint8_t>(i + 1);

        // 分配 NACK 窗口（此处为框架实现）
        // 实际实现需要集成 Token 发射器
        // emitter.send_token(node_id, NACK_WINDOW_MS);
        // 当前为占位逻辑，实际调用需要在集成阶段完成
        (void)node_id;  // 避免 unused variable 警告

        // 保护间隔（per SCHED-12, GUARD_INTERVAL = 5ms）
        // 避免节点切换时的空中撞车
        apply_guard_interval(GUARD_INTERVAL_MS);
    }
}

// 获取当前相位
BreathingPhase BreathingCycle::get_current_phase() const {
    return current_phase;
}

// 设置节点数量
void BreathingCycle::set_num_nodes(size_t n) {
    if (n > 0 && n <= 255) {
        num_nodes = n;
    }
}

// 获取周期总时长（毫秒）
// 呼出 800ms + 吸气 200ms = 1000ms
uint32_t BreathingCycle::get_cycle_duration_ms() const {
    return static_cast<uint32_t>(EXHALE_DURATION_MS) +
           static_cast<uint32_t>(INHALE_DURATION_MS);  // 1000ms
}
