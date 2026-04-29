#ifndef WFB_CONFIG_H
#define WFB_CONFIG_H

#include <string>
#include <cstdint>

// FEC 参数配置结构体
struct FECConfig {
    int n = 12;  // 总块数（数据块 + 冗余块）
    int k = 8;   // 数据块数

    // 计算冗余块数
    int redundancy() const { return n - k; }

    // 验证参数有效性
    bool is_valid() const {
        return k > 0 && n > k && n <= 255;
    }
};

// 运行时配置
struct Config {
    enum class Mode { SERVER, CLIENT } mode = Mode::SERVER;

    // 网络参数
    std::string interface;      // -i wlan0
    int channel = 6;            // -c 6
    int mcs = 0;                // -m 0
    int bandwidth = 20;         // --bw 20 (MHz)
    std::string tun_name = "wfb0";  // --tun wfb0
    uint8_t node_id = 0;        // --node-id 1 (client only)
    int node_count = 10;        // --node-count 10 (server only, 1-255)

    // FEC 参数（可配置）
    FECConfig fec;

    // 日志级别
    bool verbose = false;       // -v

    // 时间参数（毫秒）
    uint32_t min_window_ms = 50;
    uint32_t max_window_ms = 300;
    uint32_t step_up_ms = 100;
    uint32_t elastic_margin_ms = 50;
    uint32_t guard_interval_ms = 5;

    // 验证配置有效性
    bool validate() const;
};

// 解析命令行参数
bool parse_arguments(int argc, char** argv, Config& config);

// 打印用法
void print_usage(const char* program_name);

#endif  // WFB_CONFIG_H