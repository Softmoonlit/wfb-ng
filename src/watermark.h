// src/watermark.h
#pragma once
#include <cstddef>
#include <cstdint>

// 动态计算逻辑背压水位线
size_t calculate_dynamic_limit(uint16_t duration_ms, uint64_t phy_rate_bps, size_t mtu);
