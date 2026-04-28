#include "threads.h"
#include <sys/time.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <iostream>
#include <cstring>

bool set_thread_realtime_priority(int priority, const char* name) {
    // 检查优先级范围
    if (priority < 1 || priority > 99) {
        std::cerr << "错误: 实时优先级必须在 1-99 范围内" << std::endl;
        return false;
    }

    // 设置实时调度策略
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;

    // 尝试设置 SCHED_FIFO 策略
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        std::cerr << "警告: 无法设置实时调度策略 (SCHED_FIFO): "
                  << strerror(errno) << std::endl;

        // 尝试设置 SCHED_RR 作为备选
        if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
            std::cerr << "警告: 无法设置实时调度策略 (SCHED_RR): "
                      << strerror(errno) << std::endl;
            return false;
        }

        if (name) {
            std::cout << "线程 '" << name << "' 设置为 SCHED_RR 优先级 " << priority << std::endl;
        }
        return true;
    }

    if (name) {
        std::cout << "线程 '" << name << "' 设置为 SCHED_FIFO 优先级 " << priority << std::endl;
    }
    return true;
}

uint64_t get_monotonic_ms() {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
        // 回退到 gettimeofday
        struct timeval tv;
        gettimeofday(&tv, nullptr);
        return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    }
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}