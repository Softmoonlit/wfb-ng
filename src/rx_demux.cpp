#include "rx_demux.h"
#include "mac_token.h"
#include "error_handler.h"
#include "resource_guard.h"
#include "zfex.h"

#include <pcap/pcap.h>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>  // 用于le16toh

// Radiotap 头部结构
struct ieee80211_radiotap_header {
    uint8_t it_version;    // 版本，应为 0
    uint8_t it_pad;
    uint16_t it_len;       // 整个头部长度
    uint32_t it_present;   // 存在的位图
};

namespace {

// pcap 错误重试参数
constexpr int kMaxPcapRetries = 3;
constexpr int kPcapRetryDelayMs = 10;

// TokenFrame 大小常量（从 mac_token.h 继承）
constexpr size_t kTokenFrameSize = sizeof(TokenFrame);

// 解析结果
enum class ParseResult {
    OK,
    SKIP,       // 跳过此帧（非目标）
    ERROR       // 解析错误
};

// 检查是否为广播或指定节点
bool is_target_node(uint8_t target_node, uint8_t local_node_id, bool is_server) {
    // 服务端处理所有 Token（包括广播和指定节点）
    if (is_server) {
        return true;
    }

    // 客户端：0表示广播，非0表示指定节点
    if (target_node == 0) {
        return true;  // 广播 Token，所有客户端都应处理
    }

    return target_node == local_node_id;
}

// 解析单个帧
ParseResult parse_frame(
    const uint8_t* data,
    size_t len,
    TokenFrame& out_token,
    uint8_t local_node_id,
    bool is_server
) {
    // 跳过 Radiotap 头部
    if (len < sizeof(ieee80211_radiotap_header)) {
        return ParseResult::SKIP;
    }

    auto* rtap = reinterpret_cast<const ieee80211_radiotap_header*>(data);
    uint16_t rtap_len = le16toh(rtap->it_len);

    if (rtap_len < sizeof(ieee80211_radiotap_header) ||
        len < rtap_len + kTokenFrameSize) {
        return ParseResult::SKIP;
    }

    // 解析 TokenFrame
    const uint8_t* token_data = data + rtap_len;
    out_token = parse_token(token_data);

    // 验证魔数
    if (out_token.magic != 0x02) {
        return ParseResult::SKIP;
    }

    // 检查是否为发送给本节点的 Token
    if (!is_target_node(out_token.target_node, local_node_id, is_server)) {
        return ParseResult::SKIP;
    }

    return ParseResult::OK;
}

}  // namespace

pcap_t* init_pcap_handle(const char* interface, int channel, char* error_buf) {
    // 1. 创建 pcap 句柄
    pcap_t* pcap = pcap_create(interface, error_buf);
    if (!pcap) {
        return nullptr;
    }

    // 2. 设置 Monitor 模式
    int ret = pcap_set_rfmon(pcap, 1);
    if (ret != 0) {
        pcap_close(pcap);
        snprintf(error_buf, PCAP_ERRBUF_SIZE, "无法设置 Monitor 模式");
        return nullptr;
    }

    // 3. 设置超时
    pcap_set_timeout(pcap, 10);  // 10ms

    // 4. 设置缓冲区大小
    pcap_set_buffer_size(pcap, 2 * 1024 * 1024);  // 2MB

    // 5. 激活
    ret = pcap_activate(pcap);
    if (ret != 0) {
        pcap_close(pcap);
        snprintf(error_buf, PCAP_ERRBUF_SIZE, "pcap 激活失败: %s",
                 pcap_geterr(pcap));
        return nullptr;
    }

    // 6. 设置非阻塞模式（可选，便于检查 shutdown）
    pcap_setnonblock(pcap, 1, error_buf);

    return pcap;
}

RxError rx_demux_main_loop(
    ThreadSharedState* shared_state,
    const RxConfig& config,
    std::atomic<bool>* shutdown,
    RxStats* stats
) {
    char error_buf[PCAP_ERRBUF_SIZE] = {0};

    // 使用 RAII 管理 pcap 句柄
    pcap_t* raw_pcap = init_pcap_handle(config.interface, config.channel, error_buf);
    if (!raw_pcap) {
        std::cerr << "pcap 初始化失败: " << error_buf << std::endl;
        return RxError::PCAP_OPEN_FAILED;
    }
    auto pcap = make_pcap_handle(raw_pcap);

    std::cout << "RX 线程启动，接口: " << config.interface << std::endl;

    TokenFrame token;
    int consecutive_errors = 0;
    constexpr int kMaxConsecutiveErrors = 10;

    while (!shutdown->load()) {
        struct pcap_pkthdr* header;
        const u_char* data;

        // 使用 pcap_next_ex 而非 pcap_next，便于错误处理
        int ret = pcap_next_ex(pcap.get(), &header, &data);

        switch (ret) {
            case 1:  // 成功读取
                consecutive_errors = 0;
                if (stats) stats->packets_received++;

                // 解析帧
                switch (parse_frame(data, header->caplen, token,
                                   config.local_node_id, config.is_server)) {
                    case ParseResult::OK: {
                        // 获取当前时间
                        uint64_t now_ms = get_monotonic_ms();

                        // Gap-07 修复：限制最大 duration 防止溢出
                        constexpr uint16_t kMaxTokenDuration = 60000;  // 60 秒
                        uint16_t safe_duration = std::min(token.duration_ms, kMaxTokenDuration);
                        uint64_t expire_time = now_ms + safe_duration;

                        // 更新共享状态
                        {
                            std::lock_guard<std::mutex> lock(shared_state->mtx);
                            shared_state->set_token_granted(expire_time);
                        }

                        if (stats) stats->tokens_parsed++;

                        if (config.pcap_timeout_ms > 0) {
                            std::cout << "收到 Token: 节点="
                                      << static_cast<int>(token.target_node)
                                      << ", 时长=" << token.duration_ms << "ms"
                                      << ", 序列号=" << token.seq_num
                                      << std::endl;
                        }
                        break;
                    }
                    case ParseResult::SKIP:
                        // 正常跳过
                        break;
                    case ParseResult::ERROR:
                        if (stats) stats->parse_errors++;
                        break;
                }
                break;

            case 0:  // 超时，无数据
                // 正常情况，继续轮询
                break;

            case -1:  // 错误
                consecutive_errors++;
                if (stats) stats->pcap_errors++;
                std::cerr << "pcap 错误: " << pcap_geterr(pcap.get()) << std::endl;

                if (consecutive_errors >= kMaxConsecutiveErrors) {
                    std::cerr << "连续错误过多，退出 RX 线程" << std::endl;
                    return RxError::PCAP_OPEN_FAILED;
                }

                // 短暂等待后重试
                usleep(kPcapRetryDelayMs * 1000);
                break;

            case -2:  // EOF（文件读取模式，不应出现）
                return RxError::UNKNOWN_ERROR;
        }
    }

    std::cout << "RX 线程正常退出" << std::endl;
    return RxError::OK;
}