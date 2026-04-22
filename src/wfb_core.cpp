#include "threads.h"

#include <pthread.h>
#include <sched.h>
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "guard_interval.h"
#include "watermark.h"

namespace {

// 将单调时钟转换为毫秒，供门控时间判断使用。
uint64_t get_monotonic_ms() {
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rc != 0) {
        return 0;
    }
    return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
           static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
}

// 统一输出实时调度失败告警。
void warn_realtime_priority_failure(int priority, int err) {
    std::cerr << "警告: 无法设置 SCHED_FIFO 优先级 " << priority
              << ", errno=" << err << " (" << std::strerror(err) << ")" << std::endl;
}

}  // namespace

void set_thread_realtime_priority(int priority) {
    pthread_t self = pthread_self();
    struct sched_param params;
    memset(&params, 0, sizeof(params));
    params.sched_priority = priority;

    int rc = pthread_setschedparam(self, SCHED_FIFO, &params);
    if (rc != 0) {
        warn_realtime_priority_failure(priority, rc);
    }
}

void control_main_loop(ThreadSharedState* shared_state) {
    assert(shared_state != nullptr);
    set_thread_realtime_priority(kControlThreadPriority);

    std::unique_lock<std::mutex> lock(shared_state->mtx);
    shared_state->can_send = true;
    shared_state->token_expire_time_ms = get_monotonic_ms() + kGuardIntervalMs;
    shared_state->cv_send.notify_one();
}

void tx_main_loop(ThreadSharedState* shared_state) {
    assert(shared_state != nullptr);
    set_thread_realtime_priority(kTxThreadPriority);

    std::unique_lock<std::mutex> lock(shared_state->mtx);
    shared_state->cv_send.wait(lock, [shared_state]() {
        return shared_state->can_send;
    });

    uint64_t now_ms = get_monotonic_ms();
    if (shared_state->can_send && now_ms <= shared_state->token_expire_time_ms) {
        lock.unlock();
        apply_guard_interval(kGuardIntervalMs);
    } else {
        shared_state->can_send = false;
    }
}

void tun_reader_main_loop(ThreadSharedState* shared_state) {
    assert(shared_state != nullptr);

    const size_t dynamic_limit = calculate_dynamic_limit(25, kDefaultPhyRateBps, 1450);
    shared_state->packet_queue.push(static_cast<uint8_t>(dynamic_limit & 0xff));
}

int main() {
    ThreadSharedState shared_state;

    control_main_loop(&shared_state);
    tx_main_loop(&shared_state);
    tun_reader_main_loop(&shared_state);

    return 0;
}
