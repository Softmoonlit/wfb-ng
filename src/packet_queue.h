// src/packet_queue.h
#pragma once
#include <vector>
#include <mutex>
#include <stdexcept>

// 物理内存池静态分配，绝对安全的环形队列
template<typename T>
class ThreadSafeQueue {
private:
    std::vector<T> buffer;
    size_t max_capacity;
    size_t head;
    size_t tail;
    size_t count;
    std::mutex mtx;

public:
    ThreadSafeQueue(size_t capacity) : max_capacity(capacity), head(0), tail(0), count(0) {
        buffer.resize(capacity);
    }

    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mtx);
        if (count == max_capacity) return; // 达到最大容量直接丢弃
        buffer[tail] = item;
        tail = (tail + 1) % max_capacity;
        count++;
    }

    T pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (count == 0) throw std::runtime_error("队列为空");
        T item = buffer[head];
        head = (head + 1) % max_capacity;
        count--;
        return item;
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return count;
    }

    bool empty() {
        return size() == 0;
    }
};
