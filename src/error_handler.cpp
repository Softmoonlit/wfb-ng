#include "error_handler.h"

// 默认错误处理回调（什么都不做）
static ErrorCallback g_error_callback = nullptr;

void set_error_callback(ErrorCallback callback) {
    g_error_callback = callback;
}

void log_error(const ErrorContext& ctx) {
    if (g_error_callback) {
        g_error_callback(ctx);
    }
}