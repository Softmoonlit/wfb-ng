// src/watermark.cpp
#include "watermark.h"
#include <cmath>

size_t calculate_dynamic_limit(uint16_t duration_ms, uint64_t phy_rate_bps, size_t mtu) {
    // 依据当前底层物理速率和授权时间动态计算，确保 1.5 倍底仓容忍调度延迟
    double limit = (static_cast<double>(phy_rate_bps) * duration_ms * 1.5) / (8.0 * mtu * 1000.0);
    return static_cast<size_t>(std::ceil(limit));
}
