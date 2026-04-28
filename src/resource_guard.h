#ifndef WFB_RESOURCE_GUARD_H
#define WFB_RESOURCE_GUARD_H

#include <memory>
#include <functional>
#include <mutex>
#include <chrono>
#include <unistd.h>

// pcap 头文件
#include <pcap/pcap.h>

// zfex 头文件（如果可用）
#ifdef USE_ZFEC
#include "zfex.h"
#else
// zfex_code 前向声明
typedef struct zfex_code zfex_code;
// 前向声明 zfex 销毁函数
extern "C" void zfex_code_destroy(zfex_code*);
#endif

/**
 * 自定义删除器，用于智能指针管理 C 风格资源
 */
template<typename T>
struct ResourceDeleter {
    void operator()(T* ptr) const {
        if (ptr) {
            deleter(ptr);
        }
    }

    std::function<void(T*)> deleter;
};

// pcap 句柄智能指针
using PcapHandle = std::unique_ptr<pcap_t, ResourceDeleter<pcap_t>>;

// 创建 pcap 句柄
inline PcapHandle make_pcap_handle(pcap_t* raw) {
    if (!raw) return nullptr;
    return PcapHandle(raw, ResourceDeleter<pcap_t>{pcap_close});
}

// zfex_code 智能指针
struct ZfexCodeDeleter {
    void operator()(zfex_code* ptr) const {
        if (ptr) zfex_code_destroy(ptr);
    }
};
using ZfexCodeHandle = std::unique_ptr<zfex_code, ZfexCodeDeleter>;

// 创建 zfex_code 智能指针的辅助函数
inline ZfexCodeHandle make_zfex_handle(zfex_code* raw) {
    if (!raw) return nullptr;
    return ZfexCodeHandle(raw);
}

// 文件描述符智能指针
struct FdDeleter {
    void operator()(int* fd) const {
        if (fd && *fd >= 0) {
            close(*fd);
            delete fd;
        }
    }
};
using FdHandle = std::unique_ptr<int, FdDeleter>;

// 创建文件描述符智能指针的辅助函数
inline FdHandle make_fd_handle(int raw_fd) {
    if (raw_fd < 0) return nullptr;
    return FdHandle(new int(raw_fd));
}

/**
 * RAII 锁守卫，带超时
 */
class TimedMutexGuard {
public:
    explicit TimedMutexGuard(std::timed_mutex& mtx, uint32_t timeout_ms)
        : mtx_(mtx), locked_(false) {
        locked_ = mtx_.try_lock_for(std::chrono::milliseconds(timeout_ms));
    }

    ~TimedMutexGuard() {
        if (locked_) mtx_.unlock();
    }

    bool is_locked() const { return locked_; }

    TimedMutexGuard(const TimedMutexGuard&) = delete;
    TimedMutexGuard& operator=(const TimedMutexGuard&) = delete;

private:
    std::timed_mutex& mtx_;
    bool locked_;
};

#endif  // WFB_RESOURCE_GUARD_H