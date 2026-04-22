#pragma once

#include <cstdint>
#include <cstddef>

// 呼吸周期相位枚举（per SCHED-12）
// 用于标识下行广播的当前状态
enum class BreathingPhase {
    EXHALE,   // 呼出：服务器全速下发 40MB 模型
    INHALE    // 吸气：服务器暂停，分配 NACK 回传窗口
};

// 呼吸周期状态机类
// 实现服务端下行广播的时间分片机制，平衡数据下发和反馈收集
class BreathingCycle {
private:
    BreathingPhase current_phase = BreathingPhase::EXHALE;
    uint64_t phase_start_ms = 0;         // 当前相位开始时间（CLOCK_MONOTONIC 毫秒）
    size_t num_nodes = 10;               // 节点数量

    // 时间常量（per SCHED-12）
    static constexpr uint16_t EXHALE_DURATION_MS = 800;  // 呼出 800ms
    static constexpr uint16_t INHALE_DURATION_MS = 200;  // 吸气 200ms
    static constexpr uint16_t NACK_WINDOW_MS = 25;       // NACK 窗口 25ms/节点
    static constexpr uint16_t GUARD_INTERVAL_MS = 5;     // 保护间隔 5ms

public:
    // 更新呼吸周期状态，返回当前相位
    // 参数：
    //   current_time_ms - 当前时间（CLOCK_MONOTONIC 毫秒）
    // 返回值：
    //   当前呼吸相位
    // 说明：
    //   根据时间流逝自动切换相位：
    //   - EXHALE 持续 800ms 后切换到 INHALE
    //   - INHALE 持续 200ms 后切换到 EXHALE
    BreathingPhase update(uint64_t current_time_ms);

    // 执行吸气阶段：为每个节点分配 NACK 窗口
    // 说明：
    //   在吸气阶段，服务器暂停下行数据发送，
    //   依次为每个节点分配 NACK 回传窗口。
    //   每个节点窗口之间插入保护间隔，避免空中撞车。
    void execute_inhale_phase();

    // 获取当前相位
    BreathingPhase get_current_phase() const;

    // 设置节点数量
    // 参数：
    //   n - 节点数量（1-255）
    void set_num_nodes(size_t n);

    // 获取周期总时长（毫秒）
    // 返回值：
    //   EXHALE_DURATION_MS + INHALE_DURATION_MS = 1000ms
    uint32_t get_cycle_duration_ms() const;

    // 获取呼出阶段时长（毫秒）
    uint16_t get_exhale_duration_ms() const { return EXHALE_DURATION_MS; }

    // 获取吸气阶段时长（毫秒）
    uint16_t get_inhale_duration_ms() const { return INHALE_DURATION_MS; }

    // 获取 NACK 窗口时长（毫秒）
    uint16_t get_nack_window_ms() const { return NACK_WINDOW_MS; }

    // 获取保护间隔时长（毫秒）
    uint16_t get_guard_interval_ms() const { return GUARD_INTERVAL_MS; }

    // 获取节点数量
    size_t get_num_nodes() const { return num_nodes; }
};
