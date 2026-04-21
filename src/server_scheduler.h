// src/server_scheduler.h
#pragma once
#include <cstdint>
#include <map>
#include <algorithm>

// 服务端核心三态状态机调度器
class ServerScheduler {
private:
    const uint16_t MIN_WINDOW = 50;
    const uint16_t MAX_WINDOW = 300;
    const uint16_t STEP_UP = 100;
    const uint16_t ELASTIC_MARGIN = 50;

    struct NodeState {
        uint8_t silence_count = 0;
        bool in_aq = false;
    };
    std::map<uint8_t, NodeState> nodes;

public:
    void init_node(uint8_t node_id);
    // 根据实际使用的耗时和已分配的时隙计算下一轮时隙
    uint16_t calculate_next_window(uint8_t node_id, uint16_t actual_used, uint16_t allocated);
};
