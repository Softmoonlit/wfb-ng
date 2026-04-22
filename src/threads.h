#pragma once

// 线程角色与共享状态定义
// 本头文件定义 Phase 2 单进程架构中的线程入口、共享状态和关键常量。

#include <condition_variable>
#include <cstdint>
#include <mutex>

#include "packet_queue.h"

// 实时调度优先级常量（per RT-01, RT-02, RT-03）
// 控制面线程使用最高优先级 99，发射端使用次高优先级 90。
constexpr int kControlThreadPriority = 99;
constexpr int kTxThreadPriority = 90;

// 保护间隔常量（per D-16）
// 节点切换时插入 5ms 保护间隔，避免空中撞车。
constexpr uint16_t kGuardIntervalMs = 5;

// 默认物理速率（6Mbps，MCS 0 / 20MHz）
// 后续将从 MCS 配置动态映射。
constexpr uint64_t kDefaultPhyRateBps = 6000000ULL;

// 线程共享状态
// 控制面、发射端和 TUN 读取线程通过此结构共享同步对象和门控变量。
struct ThreadSharedState {
    // 发射门控：true 表示可发射，false 表示禁止发射。
    bool can_send = false;

    // Token 过期时间（单调时钟毫秒）。
    // 发射前必须检查当前时间是否小于此值。
    uint64_t token_expire_time_ms = 0;

    // 条件变量：TUN 读取线程装弹后通知发射线程。
    std::condition_variable cv_send;

    // 互斥锁：保护 can_send 和 token_expire_time_ms。
    std::mutex mtx;

    // 数据包队列：TUN 读取线程入队，发射线程出队。
    ThreadSafeQueue<uint8_t> packet_queue{100};
};

// 检查是否以 Root 权限运行（per RT-04）
// 返回值：true 表示具有 Root 权限，false 表示普通用户
// 说明：SCHED_FIFO 实时调度需要 Root 权限或 CAP_SYS_NICE 能力
bool check_root_permission();

// 实时调度提权函数（per RT-01）
// 使用 pthread_setschedparam 设置 SCHED_FIFO 策略。
// 参数：
//   priority - 实时优先级（1-99，数值越大优先级越高）
// 注意：需要 Root 权限才能成功设置实时优先级
void set_thread_realtime_priority(int priority);

// 控制面主循环（per PROC-05）
// 职责：接收控制帧、更新共享状态、触发发射线程唤醒。
void control_main_loop(ThreadSharedState* shared_state);

// 发射端主循环（per PROC-07）
// 职责：等待条件变量唤醒、检查门控、执行发射。
void tx_main_loop(ThreadSharedState* shared_state);

// TUN 读取主循环（per PROC-04）
// 职责：从 TUN 设备读取数据包、按水位线反压、入队。
// 注意：保持普通优先级，不做实时调度提权（per RT-03）
void tun_reader_main_loop(ThreadSharedState* shared_state);
