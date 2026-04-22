#pragma once
#include <stdint.h>

// 插入保护间隔（高精度休眠）
// 参数:
//   duration_ms: 保护间隔时长（毫秒）
// 实现: 使用 timerfd + 空转自旋，确保微秒级精度（per D-15）
void apply_guard_interval(uint16_t duration_ms);
