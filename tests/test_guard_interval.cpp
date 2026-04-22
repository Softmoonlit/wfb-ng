#include <cassert>
#include <iostream>
#include <ctime>
#include "../src/guard_interval.h"

void test_guard_interval_precision() {
    // 测试 5ms 保护间隔精度
    const uint16_t duration_ms = 5;
    const int64_t tolerance_ns = 100000;  // 100μs 容差

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    apply_guard_interval(duration_ms);

    clock_gettime(CLOCK_MONOTONIC, &end);

    // 计算实际延迟
    int64_t actual_ns = (end.tv_sec - start.tv_sec) * 1000000000 +
                        (end.tv_nsec - start.tv_nsec);
    int64_t expected_ns = duration_ms * 1000000;

    // 验证精度（允许 ± 100μs 误差）
    int64_t error_ns = actual_ns - expected_ns;
    if (error_ns < 0) error_ns = -error_ns;

    assert(error_ns <= tolerance_ns);

    std::cout << "PASS: 保护间隔精度正确（期望 " << duration_ms
              << "ms，实际 " << (actual_ns / 1000000.0) << "ms）" << std::endl;
}

void test_multiple_intervals() {
    // 测试多次调用延迟一致
    const int num_tests = 10;

    for (int i = 0; i < num_tests; ++i) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        apply_guard_interval(5);

        clock_gettime(CLOCK_MONOTONIC, &end);

        int64_t actual_ns = (end.tv_sec - start.tv_sec) * 1000000000 +
                            (end.tv_nsec - start.tv_nsec);

        // 验证每次调用延迟在 5ms ± 100μs 范围内
        int64_t expected_ns = 5000000;
        int64_t error_ns = actual_ns - expected_ns;
        if (error_ns < 0) error_ns = -error_ns;

        assert(error_ns <= 100000);
    }

    std::cout << "PASS: 多次调用延迟一致" << std::endl;
}

void test_different_durations() {
    // 测试不同的 duration_ms 参数
    uint16_t durations[] = {1, 5, 10, 20};

    for (uint16_t duration : durations) {
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        apply_guard_interval(duration);

        clock_gettime(CLOCK_MONOTONIC, &end);

        int64_t actual_ns = (end.tv_sec - start.tv_sec) * 1000000000 +
                            (end.tv_nsec - start.tv_nsec);
        int64_t expected_ns = duration * 1000000;

        // 验证精度（允许 ± 100μs 误差）
        int64_t error_ns = actual_ns - expected_ns;
        if (error_ns < 0) error_ns = -error_ns;

        assert(error_ns <= 100000);

        std::cout << "PASS: duration=" << duration << "ms 精度正确" << std::endl;
    }
}

int main() {
    test_guard_interval_precision();
    test_multiple_intervals();
    test_different_durations();

    std::cout << "所有保护间隔测试通过！" << std::endl;
    return 0;
}
