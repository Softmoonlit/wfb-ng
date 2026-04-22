// src/server_scheduler.h
#pragma once
#include <cstdint>
#include <map>
#include <algorithm>

// 三态状态枚举（per SCHED-01）
enum class NodeStateEnum {
    STATE_I,   // 枯竭与睡眠 (Zero-Payload Drop, < 5ms)
    STATE_II,  // 唤醒与需量拟合 (Demand-Fitting, 5ms <= t < allocated)
    STATE_III  // 高负载贪突发 (Additive Increase, t >= allocated)
};

// 前向声明
class AqSqManager;

// 服务端核心三态状态机调度器
class ServerScheduler {
private:
    // 时间参数常量（per SCHED-05, SCHED-06, SCHED-07, SCHED-08）
    const uint16_t MIN_WINDOW = 50;      // 最小授权窗口
    const uint16_t STEP_UP = 100;        // 状态 III 扩窗步长
    const uint16_t ELASTIC_MARGIN = 50;  // 状态 II 弹性边界

    // 节点状态结构（扩展版）
    struct NodeState {
        uint8_t silence_count = 0;       // Watchdog 计数器（per SCHED-02）
        NodeStateEnum current_state = NodeStateEnum::STATE_I;  // 当前状态
        uint16_t current_window = 50;    // 当前授权窗口
        bool in_aq = false;              // 是否在活跃队列中
    };

    std::map<uint8_t, NodeState> nodes;  // 节点状态映射

    AqSqManager* aq_sq_manager = nullptr;  // 队列管理器引用
    size_t num_nodes = 10;                  // 节点数量

public:
    /**
     * @brief 初始化节点状态
     * @param node_id 节点 ID（1-10）
     */
    void init_node(uint8_t node_id);

    /**
     * @brief 根据实际使用的耗时和已分配的时隙计算下一轮时隙
     * @param node_id 节点 ID
     * @param actual_used 实际使用时间（ms）
     * @param allocated 已分配的时间（ms）
     * @return 下一轮授权窗口（ms）
     */
    uint16_t calculate_next_window(uint8_t node_id, uint16_t actual_used, uint16_t allocated);

    /**
     * @brief 设置 AQ/SQ 队列管理器
     * @param manager 队列管理器指针
     */
    void set_aq_sq_manager(AqSqManager* manager);

    /**
     * @brief 设置节点数量
     * @param n 节点数量
     */
    void set_num_nodes(size_t n);

    /**
     * @brief 动态计算 MAX_WINDOW（per SCHED-06）
     * @return MAX_WINDOW 值（ms）
     *
     * 规则:
     * - 5 节点及以下: 600ms
     * - 10 节点及以下: 300ms
     * - 更多节点: 150ms
     */
    uint16_t get_max_window() const;
};
