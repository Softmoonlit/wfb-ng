#include "tx_worker.h"
#include "packet_queue.h"
#include "radiotap_template.h"
#include "error_handler.h"
#include "guard_interval.h"
#include "threads.h"
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <unistd.h>

namespace {

// IEEE 802.11 数据帧头部最小长度
constexpr size_t kMinFrameLen = 24;  // 基本数据帧头部
constexpr size_t kMaxFrameLen = 2350;  // 最大帧长（含 Radiotap）

// 适配器：将 ThreadSafeQueue<uint8_t> 转换为 TxPacket
TxPacket extract_packet_from_queue(ThreadSafeQueue<uint8_t>& queue) {
    TxPacket packet;
    try {
        uint8_t byte = queue.pop();
        // 简化：只取一个字节作为示例
        packet.data.push_back(byte);
        packet.len = 1;
    } catch (const std::runtime_error& e) {
        // 队列为空
        packet.len = 0;
    }
    return packet;
}

/**
 * 安全构建 IEEE 802.11 数据帧
 *
 * @param pkt 数据包
 * @param rtap_template Radiotap 模板
 * @param frame 输出帧缓冲区
 * @return 成功返回帧长度，失败返回 0
 */
size_t build_frame_safe(const TxPacket& pkt,
                        const RadiotapTemplate& rtap_template,
                        std::vector<uint8_t>& frame) {
    // 参数验证
    if (pkt.len == 0 || pkt.len > 1450) {
        std::cerr << "无效数据包长度: " << pkt.len << std::endl;
        return 0;
    }

    // 清空输出缓冲区
    frame.clear();
    frame.reserve(kMaxFrameLen);

    // Radiotap 头部
    const uint8_t* rtap_data = rtap_template.get_template();
    uint16_t rtap_len = rtap_template.get_length();

    if (!rtap_data || rtap_len == 0) {
        std::cerr << "Radiotap 模板未初始化" << std::endl;
        return 0;
    }

    frame.insert(frame.end(), rtap_data, rtap_data + rtap_len);

    // IEEE 802.11 数据帧头部
    // Frame Control: Data frame, To DS
    frame.push_back(0x08);  // Data frame
    frame.push_back(0x01);  // To DS

    // Duration (2 bytes)
    frame.push_back(0x00);
    frame.push_back(0x00);

    // Address 1-3 (各 6 字节)
    // Address 1: BSSID（使用广播地址简化）
    for (int i = 0; i < 6; i++) frame.push_back(0xFF);
    // Address 2: SA（源地址）
    for (int i = 0; i < 6; i++) frame.push_back(0x00);
    // Address 3: DA（目的地址）
    for (int i = 0; i < 6; i++) frame.push_back(0xFF);

    // Sequence Control (2 bytes)
    frame.push_back(0x00);
    frame.push_back(0x00);

    // 数据载荷
    frame.insert(frame.end(), pkt.data.begin(), pkt.data.begin() + pkt.len);

    // 验证帧长度
    if (frame.size() > kMaxFrameLen) {
        std::cerr << "帧过长: " << frame.size() << " bytes" << std::endl;
        return 0;
    }

    return frame.size();
}

/**
 * 带重试的 pcap_inject
 *
 * @param pcap_handle pcap 句柄
 * @param data 数据指针
 * @param len 数据长度
 * @param max_retries 最大重试次数
 * @param retry_delay_us 重试延迟（微秒）
 * @return 成功返回实际发送字节数，失败返回 -1
 */
int pcap_inject_retry(pcap_t* pcap_handle,
                      const uint8_t* data,
                      size_t len,
                      int max_retries,
                      uint32_t retry_delay_us) {
    for (int retry = 0; retry < max_retries; retry++) {
        int ret = pcap_inject(pcap_handle, data, len);
        if (ret >= 0) {
            return ret;
        }

        // 检查是否可重试
        int err = pcap_geterr(pcap_handle) ? errno : 0;
        if (err == EAGAIN || err == EWOULDBLOCK || err == ENOBUFS) {
            // 可重试错误
            if (retry < max_retries - 1) {
                usleep(retry_delay_us);
                continue;
            }
        }

        // 不可重试或重试次数用尽
        std::cerr << "pcap_inject 失败 (retry " << retry + 1 << "/"
                  << max_retries << "): " << pcap_geterr(pcap_handle) << std::endl;
        return -1;
    }

    return -1;
}

}  // namespace
TxError tx_main_loop(
    ThreadSharedState* shared_state,
    pcap_t* pcap_handle,
    const TxConfig& config,
    std::atomic<bool>* shutdown,
    TxStats* stats
) {
    // 参数验证
    if (!shared_state || !pcap_handle || !shutdown) {
        return TxError::THREAD_SHUTDOWN;
    }

    // 初始化 Radiotap 模板
    RadiotapTemplate rtap_template;
    rtap_template.init_for_data(config.mcs, config.bandwidth);

    // 批量发射缓冲区
    std::vector<TxPacket> batch;
    batch.reserve(config.batch_size);

    // 帧构建缓冲区
    std::vector<uint8_t> frame;
    frame.reserve(kMaxFrameLen);

    std::cout << "TX 线程启动，MCS=" << config.mcs 
              << ", 批量大小=" << config.batch_size << std::endl;

    while (!shutdown->load()) {
        // 零轮询唤醒：等待 can_send 或 shutdown
        std::unique_lock<std::mutex> lock(shared_state->mtx);
        shared_state->cv_send.wait(lock, [&]() {
            return shared_state->can_send || shutdown->load();
        });

        if (shutdown->load()) break;

        // 门控检查
        uint64_t now_ms = get_monotonic_ms();
        if (!shared_state->can_send || 
            now_ms >= shared_state->token_expire_time_ms) {
            // Token 无效或已过期
            shared_state->can_send = false;
            continue;
        }

        // 计算剩余授权时间
        uint64_t remaining_ms = shared_state->token_expire_time_ms - now_ms;

        // 解锁，准备发射
        lock.unlock();

        // 批量取包
        batch.clear();
        TxPacket pkt;
        for (uint32_t i = 0; i < config.batch_size; i++) {
            pkt = extract_packet_from_queue(shared_state->packet_queue);
            if (pkt.len == 0) break;
            batch.push_back(std::move(pkt));
        }

        // 发射数据包
        for (size_t i = 0; i < batch.size(); i++) {
            // 检查 Token 是否过期
            if (get_monotonic_ms() >= shared_state->token_expire_time_ms) {
                // Token 过期，停止发射
                if (stats) stats->token_expired++;
                break;
            }

            // 构建帧
            size_t frame_len = build_frame_safe(batch[i], rtap_template, frame);
            if (frame_len == 0) {
                if (stats) stats->packets_failed++;
                continue;
            }

            // 发射（带重试）
            int ret = pcap_inject_retry(
                pcap_handle, 
                frame.data(), 
                frame_len,
                config.inject_retries,
                config.inject_retry_delay_us
            );

            if (ret < 0) {
                if (stats) stats->packets_failed++;
                continue;
            }

            if (stats) stats->packets_sent++;
        }

        // 应用保护间隔（微间隔，避免连续发射拥塞）
        apply_guard_interval(1);  // 1ms 保护间隔
    }

    std::cout << "TX 线程正常退出" << std::endl;
    return TxError::OK;
}
