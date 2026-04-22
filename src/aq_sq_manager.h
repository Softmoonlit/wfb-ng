#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <utility>

// AQ/SQ 双队列管理器
// 区分活跃节点（Active Queue）和睡眠节点（Sleep Queue），支持混合交织调度。
// 线程安全设计，所有公有方法使用互斥锁保护。
class AqSqManager {
public:
    // 无效节点标识（per SCHED-10）
    static constexpr uint8_t INVALID_NODE = 0xFF;

    // 默认混合交织比例（per SCHED-10: 介于 3-5 之间）
    static constexpr int DEFAULT_INTERLEAVE_RATIO = 4;

    /**
     * @brief 初始化节点到睡眠队列
     * @param node_id 节点 ID（1-10）
     *
     * 新节点默认进入 SQ，等待激活。
     */
    void init_node(uint8_t node_id);

    /**
     * @brief 迁移节点到活跃队列
     * @param node_id 节点 ID
     *
     * 若节点已在 AQ，则幂等返回。
     * 若节点在 SQ，则从 SQ 移除并加入 AQ。
     */
    void migrate_to_aq(uint8_t node_id);

    /**
     * @brief 迁移节点到睡眠队列
     * @param node_id 节点 ID
     *
     * 若节点已在 SQ，则幂等返回。
     * 若节点在 AQ，则从 AQ 移除并加入 SQ。
     */
    void migrate_to_sq(uint8_t node_id);

    /**
     * @brief 获取下一个要服务的节点（混合交织调度）
     * @param interleave_ratio 混合交织比例（默认 4）
     * @return std::pair<uint8_t, bool> - {node_id, is_from_sq}
     *         is_from_sq=true 表示从 SQ 获取（微探询），false 表示从 AQ 获取
     *
     * 调度策略（per SCHED-10）:
     * - 全空: 返回 {INVALID_NODE, false}
     * - AQ 空 SQ 不空: 空闲巡逻模式，从 SQ 轮询
     * - 混合交织: 每服务 interleave_ratio 次 AQ，穿插 1 次 SQ
     */
    std::pair<uint8_t, bool> get_next_node(int interleave_ratio = DEFAULT_INTERLEAVE_RATIO);

    /**
     * @brief 检查活跃队列是否为空
     * @return true 表示 AQ 为空
     */
    bool is_aq_empty();

    /**
     * @brief 获取活跃队列大小
     * @return AQ 节点数量
     */
    size_t aq_size();

    /**
     * @brief 获取睡眠队列大小
     * @return SQ 节点数量
     */
    size_t sq_size();

private:
    // 活跃队列：存储当前活跃节点的 ID
    std::deque<uint8_t> aq;

    // 睡眠队列：存储当前睡眠节点的 ID
    std::deque<uint8_t> sq;

    // 快速查找集合：避免 O(n) 遍历
    std::unordered_set<uint8_t> in_aq_set;
    std::unordered_set<uint8_t> in_sq_set;

    // 线程安全保护
    std::mutex mtx;

    // 混合交织计数器：记录已服务的 AQ 节点次数
    int aq_served = 0;
};
