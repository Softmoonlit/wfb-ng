#include "guard_interval.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <cstring>
#include <iostream>

void apply_guard_interval(uint16_t duration_ms) {
    // 高精度休眠实现：
    // 1. 使用 timerfd 创建定时器
    // 2. 设置定时器在 duration_ms 后到期
    // 3. 在到期前极短时间（< 100μs）空转自旋（per D-15）
    // 4. 确保微秒级精度

    // 创建 timerfd
    int timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (timer_fd < 0) {
        std::cerr << "Error: timerfd_create failed" << std::endl;
        return;
    }

    // 设置定时器
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    its.it_value.tv_sec = duration_ms / 1000;
    its.it_value.tv_nsec = (duration_ms % 1000) * 1000000;

    if (timerfd_settime(timer_fd, 0, &its, nullptr) < 0) {
        std::cerr << "Error: timerfd_settime failed" << std::endl;
        close(timer_fd);
        return;
    }

    // 等待定时器到期
    // 方法 1: 使用 poll() 等待（阻塞，精度约 1ms）
    // 方法 2: 在截止前极短时间空转自旋（精度 < 100μs）（per D-15）

    // 获取当前时间
    struct timespec now, deadline;
    clock_gettime(CLOCK_MONOTONIC, &now);
    deadline.tv_sec = now.tv_sec + its.it_value.tv_sec;
    deadline.tv_nsec = now.tv_nsec + its.it_value.tv_nsec;

    // 处理纳秒溢出
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000;
    }

    // 粗等待：使用 poll() 阻塞到截止前 100μs
    uint64_t remaining_ns = (deadline.tv_sec - now.tv_sec) * 1000000000 +
                            (deadline.tv_nsec - now.tv_nsec);
    uint64_t coarse_ns = remaining_ns - 100000;  // 截止前 100μs

    if (coarse_ns > 100000) {  // 如果剩余时间 > 100μs
        struct pollfd pfd = { .fd = timer_fd, .events = POLLIN };
        uint64_t wait_ms = coarse_ns / 1000000;  // 转换为毫秒

        // 使用 poll 粗等待
        poll(&pfd, 1, wait_ms);
    }

    // 细等待：空转自旋到截止时间（精度 < 100μs）（per D-15）
    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec && now.tv_nsec >= deadline.tv_nsec)) {
            break;
        }
        // CPU 空转自旋
    }

    close(timer_fd);
}
