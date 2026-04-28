#ifndef WFB_ERROR_HANDLER_H
#define WFB_ERROR_HANDLER_H

#include <iostream>
#include <sstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>  // for usleep

// 错误严重级别
enum class ErrorSeverity {
    WARNING,    // 警告，继续执行
    ERROR,      // 错误，尝试恢复
    FATAL       // 致命错误，退出
};

// 错误上下文
struct ErrorContext {
    const char* function;
    const char* file;
    int line;
    int errno_value;
    std::string message;
    ErrorSeverity severity;
};

// 错误处理回调
using ErrorCallback = void(*)(const ErrorContext& ctx);

// 设置错误处理回调
void set_error_callback(ErrorCallback callback);

// 记录错误
void log_error(const ErrorContext& ctx);

// 带重试的错误处理宏
#define RETRY_ON_ERROR(expr, max_retries, delay_ms) \
    ({ \
        int __ret = -1; \
        int __retries = 0; \
        while (__retries < (max_retries)) { \
            __ret = (expr); \
            if (__ret >= 0) break; \
            usleep((delay_ms) * 1000); \
            __retries++; \
        } \
        __ret; \
    })

// 系统调用错误检查宏
#define CHECK_SYSCALL(call, msg) \
    ({ \
        int __ret = (call); \
        if (__ret < 0) { \
            ErrorContext __ctx = { \
                .function = __func__, \
                .file = __FILE__, \
                .line = __LINE__, \
                .errno_value = errno, \
                .message = std::string(msg) + ": " + strerror(errno), \
                .severity = ErrorSeverity::ERROR \
            }; \
            log_error(__ctx); \
        } \
        __ret; \
    })

// 资源创建失败检查
#define CHECK_RESOURCE(ptr, msg) \
    if (!(ptr)) { \
        ErrorContext __ctx = { \
            .function = __func__, \
            .file = __FILE__, \
            .line = __LINE__, \
            .errno_value = 0, \
            .message = std::string(msg) + ": 资源创建失败", \
            .severity = ErrorSeverity::FATAL \
        }; \
        log_error(__ctx); \
        return false; \
    }

#endif  // WFB_ERROR_HANDLER_H