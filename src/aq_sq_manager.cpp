#include "aq_sq_manager.h"

#include <algorithm>

void AqSqManager::init_node(uint8_t node_id) {
    std::lock_guard<std::mutex> lock(mtx);

    // 若已在任一队列中，幂等返回
    if (in_aq_set.count(node_id) > 0 || in_sq_set.count(node_id) > 0) {
        return;
    }

    // 新节点加入 SQ 队尾
    sq.push_back(node_id);
    in_sq_set.insert(node_id);
}

void AqSqManager::migrate_to_aq(uint8_t node_id) {
    std::lock_guard<std::mutex> lock(mtx);

    // 若已在 AQ，幂等返回
    if (in_aq_set.count(node_id) > 0) {
        return;
    }

    // 若在 SQ，先移除
    if (in_sq_set.count(node_id) > 0) {
        sq.erase(std::remove(sq.begin(), sq.end(), node_id), sq.end());
        in_sq_set.erase(node_id);
    }

    // 加入 AQ 队尾
    aq.push_back(node_id);
    in_aq_set.insert(node_id);
}

void AqSqManager::migrate_to_sq(uint8_t node_id) {
    std::lock_guard<std::mutex> lock(mtx);

    // 若已在 SQ，幂等返回
    if (in_sq_set.count(node_id) > 0) {
        return;
    }

    // 若在 AQ，先移除
    if (in_aq_set.count(node_id) > 0) {
        aq.erase(std::remove(aq.begin(), aq.end(), node_id), aq.end());
        in_aq_set.erase(node_id);
    }

    // 加入 SQ 队尾
    sq.push_back(node_id);
    in_sq_set.insert(node_id);
}

std::pair<uint8_t, bool> AqSqManager::get_next_node(int interleave_ratio) {
    std::lock_guard<std::mutex> lock(mtx);

    // 场景 1: 全空 → 返回无效节点
    if (aq.empty() && sq.empty()) {
        return {INVALID_NODE, false};
    }

    // 场景 2: AQ 空 SQ 不空 → 空闲巡逻模式
    if (aq.empty() && !sq.empty()) {
        // 从 SQ 队首取节点
        uint8_t node_id = sq.front();
        sq.pop_front();
        // 放回队尾（轮询）
        sq.push_back(node_id);
        return {node_id, true};
    }

    // 场景 3: 混合交织调度
    // 若 SQ 为空或未达到交织阈值，服务 AQ
    if (sq.empty() || aq_served < interleave_ratio) {
        // 从 AQ 队首取节点
        uint8_t node_id = aq.front();
        aq.pop_front();
        // 放回队尾（轮询）
        aq.push_back(node_id);
        aq_served++;
        return {node_id, false};
    }

    // 达到交织阈值，服务 SQ 并重置计数器
    uint8_t node_id = sq.front();
    sq.pop_front();
    sq.push_back(node_id);
    aq_served = 0;
    return {node_id, true};
}

bool AqSqManager::is_aq_empty() {
    std::lock_guard<std::mutex> lock(mtx);
    return aq.empty();
}

size_t AqSqManager::aq_size() {
    std::lock_guard<std::mutex> lock(mtx);
    return aq.size();
}

size_t AqSqManager::sq_size() {
    std::lock_guard<std::mutex> lock(mtx);
    return sq.size();
}
