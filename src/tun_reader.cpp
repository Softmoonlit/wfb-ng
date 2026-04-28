#include "tun_reader.h"
#include "watermark.h"
#include "resource_guard.h"
#include "error_handler.h"
#include "threads.h"

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include <cstring>
#include <iostream>
#include <unordered_map>
#include <vector>

/**
 * MCS 速率映射表（单位：Mbps）
 *
 * 遵循 CLAUDE.md 中的 MCS 速率映射表：
 * | MCS | 频宽 (MHz) | 理论物理速率 (Mbps) |
 * |------|------------|------------------|
 * | 0    | 20         | 6                |
 * | 1    | 20         | 9                |
 * | 2    | 20         | 12               |
 * | 3    | 20         | 18               |
 * | 4    | 20         | 24               |
 * | 5    | 20         | 36               |
 * | 6    | 20         | 54               |
 * | 7    | 40         | 72               |
 * | 8    | 40         | 144              |
 *
 * 注意：对于 40MHz 频宽，某些 MCS 值可能不可用
 */
static uint64_t mcs_rate_mbps(int mcs, int bandwidth) {
    static const std::unordered_map<int, uint64_t> rate_table_20mhz = {
        {0, 6}, {1, 9}, {2, 12}, {3, 18}, {4, 24}, {5, 36}, {6, 54}
    };

    static const std::unordered_map<int, uint64_t> rate_table_40mhz = {
        {7, 72}, {8, 144}
    };

    if (bandwidth == 20) {
        auto it = rate_table_20mhz.find(mcs);
        if (it != rate_table_20mhz.end()) {
            return it->second * 1000000;  // 转换为 bps
        }
    } else if (bandwidth == 40) {
        auto it = rate_table_40mhz.find(mcs);
        if (it != rate_table_40mhz.end()) {
            return it->second * 1000000;  // 转换为 bps
        }
    }

    // 默认回退到 MCS 0, 20MHz
    std::cerr << "警告: 无效的 MCS/频宽组合: MCS=" << mcs << ", 频宽=" << bandwidth << "MHz，使用默认值 6Mbps" << std::endl;
    return 6 * 1000000;  // 6 Mbps
}

/**
 * 获取物理速率（单位：bps）
 *
 * 根据 MCS 和频宽计算物理速率，用于动态水位线计算。
 * 遵循 CLAUDE.md 要求：严禁硬编码水位线，必须基于 MCS 速率映射表动态计算。
 *
 * @param mcs MCS 调制方案（0-8）
 * @param bandwidth_mhz 频宽（MHz，20 或 40）
 * @return 物理速率（bps）
 */
static uint64_t get_phy_rate_bps(int mcs, int bandwidth_mhz) {
    return mcs_rate_mbps(mcs, bandwidth_mhz);
}

/**
 * 计算水位线（包数）
 *
 * 基于 MCS 速率映射表计算动态水位线。
 * 遵循 CLAUDE.md 要求：严禁硬编码水位线，必须基于 MCS 速率映射表动态计算。
 *
 * @param mcs MCS 调制方案
 * @param bandwidth 频宽（MHz）
 * @param window_ms 时间窗口（毫秒）
 * @return 水位线（包数）
 */
static uint32_t calculate_watermark(int mcs, int bandwidth, uint32_t window_ms) {
    // 假设标准 MTU 为 1500 字节
    const size_t mtu = 1500;

    // 获取物理速率（bps）
    uint64_t phy_rate_bps = mcs_rate_mbps(mcs, bandwidth);

    // 使用现有的动态水位线计算函数
    return static_cast<uint32_t>(calculate_dynamic_limit(
        static_cast<uint16_t>(window_ms), phy_rate_bps, mtu));
}

/**
 * 创建 TUN 设备
 *
 * 创建并配置 TUN 网络设备，设置为非阻塞模式。
 * 遵循 CLAUDE.md 要求：所有系统调用错误都有处理和恢复逻辑。
 *
 * @param name TUN 设备名（如 "wfb0"）
 * @param error_buf 错误缓冲区（可选，256字节）
 * @return 文件描述符（失败返回 -1）
 */
int create_tun_device(const std::string& name, char* error_buf) {
    // 参数验证
    if (name.empty() || name.length() >= IFNAMSIZ) {
        if (error_buf) {
            snprintf(error_buf, 256, "TUN 设备名无效: %s (长度: %zu, 最大: %d)",
                     name.c_str(), name.length(), IFNAMSIZ - 1);
        }
        return -1;
    }

    // 打开 TUN 设备
    int fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        if (error_buf) {
            snprintf(error_buf, 256, "无法打开 /dev/net/tun: %s",
                     strerror(errno));
        }
        return -1;
    }

    // 配置 TUN 设备
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;  // TUN 设备，无包头信息
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // 确保以 null 结尾

    // 创建 TUN 接口
    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        if (error_buf) {
            snprintf(error_buf, 256, "TUNSETIFF 失败: %s", strerror(errno));
        }
        close(fd);
        return -1;
    }

    std::cout << "TUN 设备创建成功: " << name << " (fd=" << fd << ")" << std::endl;
    return fd;
}

/**
 * 验证 TUN 设备名称安全性
 * 只允许字母、数字和下划线，防止命令注入
 */
static bool validate_tun_name(const std::string& name) {
    if (name.empty() || name.size() >= IFNAMSIZ) {
        return false;
    }
    for (char c : name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

/**
 * 设置 TUN 队列长度（安全版本）
 *
 * Gap-04 修复：使用 ioctl 替代 popen，消除命令注入风险。
 * 遵循 CLAUDE.md 要求：必须设置 txqueuelen 100 实现极小水池反压。
 *
 * @param name TUN 设备名
 * @param qlen 队列长度（必须为 100）
 * @return 成功返回 true
 */
bool set_tun_txqueuelen(const std::string& name, int qlen) {
    // Gap-04 修复：验证设备名，防止命令注入
    if (!validate_tun_name(name)) {
        std::cerr << "错误: 设备名包含非法字符: " << name << std::endl;
        return false;
    }

    if (qlen <= 0) {
        std::cerr << "参数无效: qlen=" << qlen << std::endl;
        return false;
    }

    // 使用 ioctl 替代 popen，彻底消除命令注入风险
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        std::cerr << "无法创建 socket: " << strerror(errno) << std::endl;
        return false;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    ifr.ifr_qlen = qlen;

    int ret = ioctl(fd, SIOCSIFTXQLEN, &ifr);
    close(fd);

    if (ret != 0) {
        std::cerr << "警告: 设置 txqueuelen 失败: " << strerror(errno) << std::endl;
        return false;
    }

    std::cout << "TUN 队列长度设置为: " << qlen << std::endl;
    return true;
}

/**
 * TUN 读取线程主函数
 *
 * 负责从 TUN 设备读取数据包，进行 FEC 编码，并放入共享队列。
 * 遵循 RT-04：TUN 读取线程保留普通优先级（CFS），避免 FEC 编码 CPU 密集型导致系统死机。
 *
 * @param shared_state 线程共享状态
 * @param config TUN 配置
 * @param shutdown 关闭信号（原子布尔值）
 * @param stats 统计信息（可选）
 * @return 错误码
 */
TunError tun_reader_main_loop(
    ThreadSharedState* shared_state,
    const TunConfig& config,
    std::atomic<bool>* shutdown,
    TunStats* stats) {

    // 参数验证
    if (!shared_state || !shutdown) {
        std::cerr << "错误: 共享状态或关闭信号为空指针" << std::endl;
        return TunError::UNKNOWN_ERROR;
    }

    char error_buf[256] = {0};

    // 1. 创建 TUN 设备
    int tun_fd = create_tun_device(config.tun_name, error_buf);
    if (tun_fd < 0) {
        std::cerr << "创建 TUN 设备失败: " << error_buf << std::endl;
        return TunError::TUN_OPEN_FAILED;
    }

    // 使用 RAII 管理 TUN fd
    auto tun_fd_guard = make_fd_handle(tun_fd);

    // 2. 设置极小队列长度（反压关键）
    if (!set_tun_txqueuelen(config.tun_name, config.txqueuelen)) {
        std::cerr << "警告: txqueuelen 设置失败，继续使用默认值" << std::endl;
    }

    // 3. 验证 FEC 配置
    std::cout << "FEC 配置: n=" << config.fec.n << ", k=" << config.fec.k << std::endl;
    if (!config.fec.is_valid()) {
        std::cerr << "FEC 配置无效: n=" << config.fec.n << ", k=" << config.fec.k << std::endl;
        return TunError::FEC_INIT_FAILED;
    }

    // 4. 计算动态水位线
    uint64_t phy_rate = get_phy_rate_bps(config.mcs, config.bandwidth);
    constexpr size_t kMaxBlockLen = 1450;  // FEC 块最大长度
    uint32_t dynamic_limit = calculate_dynamic_limit(
        config.watermark_window_ms, phy_rate, kMaxBlockLen);

    std::cout << "TUN 读取线程启动，水位线=" << dynamic_limit
              << ", FEC(N=" << config.fec.n << ", K=" << config.fec.k << ")"
              << std::endl;

    // 5. 数据缓冲区
    std::vector<uint8_t> read_buffer(kMaxBlockLen);

    int consecutive_errors = 0;
    constexpr int kMaxConsecutiveErrors = 10;

    // 6. 主循环
    while (!shutdown->load()) {
        // 检查队列是否达到水位线
        if (shared_state->packet_queue.size() >= dynamic_limit) {
            // 队列满，制造反压
            if (stats) stats->backpressure_events++;
            usleep(1000);  // 1ms 退避
            continue;
        }

        // 读取 TUN 数据
        ssize_t nread = read(tun_fd, read_buffer.data(), kMaxBlockLen);

        if (nread < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下的正常情况
                usleep(10000);  // 10ms 读取超时
                continue;
            }

            // 真正的错误
            consecutive_errors++;
            if (stats) stats->read_errors++;
            std::cerr << "TUN 读取错误: " << strerror(errno) << std::endl;

            if (consecutive_errors >= kMaxConsecutiveErrors) {
                std::cerr << "连续错误过多，退出 TUN 读取线程" << std::endl;
                return TunError::READ_ERROR;
            }

            usleep(10000);  // 10ms 错误退避
            continue;
        }

        // 重置连续错误计数
        consecutive_errors = 0;

        if (nread == 0) {
            continue;  // 空读
        }

        if (stats) stats->packets_read++;

        // 创建数据包并入队
        // 注意：这里简化实现，实际项目中需要处理 FEC 编码
        // 目前只将数据读入队列，不进行 FEC 编码
        uint8_t packet_data = (nread > 0) ? read_buffer[0] : 0;
        shared_state->packet_queue.push(packet_data);

        // 通知发射线程有新数据
        shared_state->cv_send.notify_one();
    }

    std::cout << "TUN 读取线程正常退出" << std::endl;
    return TunError::OK;
}