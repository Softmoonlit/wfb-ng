#include "config.h"
#include <getopt.h>
#include <iostream>
#include <cstdlib>

bool Config::validate() const {
    if (interface.empty()) {
        std::cerr << "错误: 必须指定网络接口 (-i)" << std::endl;
        return false;
    }

    if (!fec.is_valid()) {
        std::cerr << "错误: FEC 参数无效 (N=" << fec.n
                  << ", K=" << fec.k << ")" << std::endl;
        return false;
    }

    if (mode == Mode::CLIENT && node_id == 0) {
        std::cerr << "错误: 客户端模式必须指定节点 ID (--node-id)" << std::endl;
        return false;
    }

    return true;
}

bool parse_arguments(int argc, char** argv, Config& config) {
    static struct option long_options[] = {
        {"mode",     required_argument, nullptr, 'M'},
        {"tun",      required_argument, nullptr, 'T'},
        {"bw",       required_argument, nullptr, 'B'},
        {"node-id",  required_argument, nullptr, 'N'},
        {"fec-n",    required_argument, nullptr, 'F'},
        {"fec-k",    required_argument, nullptr, 'K'},
        {"help",     no_argument,       nullptr, 'h'},
        {nullptr,    0,                 nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:c:m:vh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i': config.interface = optarg; break;
            case 'c': config.channel = std::stoi(optarg); break;
            case 'm': config.mcs = std::stoi(optarg); break;
            case 'v': config.verbose = true; break;
            case 'M':
                config.mode = (std::string(optarg) == "server")
                              ? Config::Mode::SERVER
                              : Config::Mode::CLIENT;
                break;
            case 'T': config.tun_name = optarg; break;
            case 'B': config.bandwidth = std::stoi(optarg); break;
            case 'N': config.node_id = static_cast<uint8_t>(std::stoi(optarg)); break;
            case 'F': config.fec.n = std::stoi(optarg); break;
            case 'K': config.fec.k = std::stoi(optarg); break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                return false;
        }
    }

    return config.validate();
}

void print_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]\n\n"
              << "模式选择:\n"
              << "  --mode server|client    运行模式（必需）\n\n"
              << "网络参数:\n"
              << "  -i <接口>               WiFi 网卡接口（必需）\n"
              << "  -c <信道>               信道号（默认: 6）\n"
              << "  -m <MCS>                MCS 调制方案 0-8（默认: 0）\n"
              << "  --bw <MHz>              频宽 20/40（默认: 20）\n"
              << "  --tun <名称>            TUN 设备名（默认: wfb0）\n\n"
              << "客户端参数:\n"
              << "  --node-id <ID>          节点 ID 1-255（客户端必需）\n\n"
              << "FEC 参数:\n"
              << "  --fec-n <N>             FEC 总块数（默认: 12）\n"
              << "  --fec-k <K>             FEC 数据块数（默认: 8）\n\n"
              << "其他:\n"
              << "  -v                      详细日志\n"
              << "  -h, --help              显示帮助\n";
}