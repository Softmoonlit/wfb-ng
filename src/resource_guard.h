#ifndef WFB_RESOURCE_GUARD_H
#define WFB_RESOURCE_GUARD_H

#include <memory>
#include <functional>
#include <mutex>

// pcap 前向声明
struct pcap;
typedef struct pcap pcap_t;

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

// 文件描述符智能指针
struct FdDeleter {
    void operator()(int* fd) const {
        if (fd && *fd >= 0) {
            close(*fd);
        }
    }
};
using FdHandle = std::unique_ptr<int, FdDeleter>;

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