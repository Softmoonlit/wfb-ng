#ifndef WFB_THREADS_H
#define WFB_THREADS_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstdint>

#include "packet_queue.h"

// 线程安全注解宏（编译时检查，Clang 支持）
#if defined(__clang__)
#define GUARDED_BY(x) __attribute__((guarded_by(x)))
#define REQUIRES(x) __attribute__((requires_capability(x)))
#define REQUIRES_SHARED(x) __attribute__((requires_shared_capability(x)))
#else
#define GUARDED_BY(x)
#define REQUIRES(x)
#define REQUIRES_SHARED(x)
#endif

/**
 * 线程共享状态
 *
 * 所有跨线程共享的数据都在此结构中管理。
 * 使用 mutex 保护非原子变量，使用条件变量实现零轮询唤醒。
 *
 * 锁定顺序约定：
 * 1. 永远不要在持有多个锁时调用外部函数
 * 2. 临界区代码应尽可能短
 * 3. 禁止在临界区内进行 I/O 操作
 */
struct ThreadSharedState {
    // 主互斥锁，保护以下 GUARDED_BY 变量
    mutable std::mutex mtx;

    // 发射控制变量
    bool can_send GUARDED_BY(mtx) = false;
    uint64_t token_expire_time_ms GUARDED_BY(mtx) = 0;

    // 动态水位线
    uint32_t dynamic_queue_limit GUARDED_BY(mtx) = 0;

    // 发射唤醒条件变量
    // 使用模式：wait(lock, predicate) 或 notify_one/all
    std::condition_variable cv_send;

    // 数据包队列（内部线程安全，不需要 mtx 保护）
    ThreadSafeQueue<uint8_t> packet_queue;

    // 全局关闭信号（原子操作，无需锁）
    std::atomic<bool> shutdown{false};

    // 统计信息（可选，用于调试）
    struct Stats {
        std::atomic<uint64_t> packets_sent{0};
        std::atomic<uint64_t> packets_dropped{0};
        std::atomic<uint64_t> tokens_received{0};
    } stats;

    // 辅助方法

    /**
     * 设置 Token 授权状态
     * 必须在持有 mtx 时调用
     */
    void set_token_granted(uint64_t expire_time_ms) REQUIRES(mtx) {
        can_send = true;
        token_expire_time_ms = expire_time_ms;
        cv_send.notify_one();
    }

    /**
     * 清除 Token 授权状态
     * 必须在持有 mtx 时调用
     */
    void clear_token() REQUIRES(mtx) {
        can_send = false;
    }

    /**
     * 检查 Token 是否有效
     * 必须在持有 mtx 时调用
     */
    bool is_token_valid(uint64_t now_ms) const REQUIRES_SHARED(mtx) {
        return can_send && (now_ms < token_expire_time_ms);
    }
};

// 线程优先级常量
namespace ThreadPriority {
    constexpr int CONTROL_PLANE = 99;   // 控制面线程（最高）
    constexpr int SCHEDULER = 95;       // 调度器线程
    constexpr int TX_WORKER = 90;       // 发射线程
    constexpr int CFS_DEFAULT = 0;      // 普通线程（CFS）
}

// 设置线程实时优先级
bool set_thread_realtime_priority(int priority, const char* name = nullptr);

// 获取单调时钟时间（毫秒）
uint64_t get_monotonic_ms();

#endif  // WFB_THREADS_H