#!/bin/bash
# WFB-NG 服务器启动脚本
# 包含系统检查、配置加载、调度器启动
# 实现 APP-03, APP-04 需求
set -e

# ============================================
# 配置参数
# ============================================
WLAN_IFACE="${WLAN_IFACE:-wlan0}"           # 无线网卡接口
MCS_INDEX="${MCS_INDEX:-6}"                  # MCS 索引（0-8）
BANDWIDTH="${BANDWIDTH:-20}"                 # 频宽（MHz）
NUM_NODES="${NUM_NODES:-10}"                 # 节点数量
TUN_NAME="${TUN_NAME:-tun0}"                 # TUN 设备名称
LOG_FILE="${LOG_FILE:-/var/log/wfb-ng/server.log}"  # 日志文件路径
BAND="${BAND:-5G}"                           # 频段（2G/5G）
CHANNEL2G="${CHANNEL2G:-6}"                  # 2G 频道
CHANNEL5G="${CHANNEL5G:-149}"                # 5G 频道

# ============================================
# Root 权限检查
# ============================================
check_root() {
    if [[ $EUID -ne 0 ]]; then
        echo "错误: 必须以 Root 权限运行此脚本" >&2
        echo "提示: 使用 sudo $0" >&2
        exit 1
    fi
    echo "检查通过: Root 权限验证成功"
}

# ============================================
# 网卡 Monitor 模式检查
# ============================================
check_monitor_mode() {
    local iface="$1"

    # 检查网卡是否存在
    if ! ip link show "$iface" &>/dev/null; then
        echo "错误: 网卡 $iface 不存在" >&2
        echo "提示: 请检查网卡名称是否正确" >&2
        exit 1
    fi

    # 检查是否处于 Monitor 模式
    if ! iwconfig "$iface" 2>&1 | grep -q "Mode:Monitor"; then
        echo "错误: 网卡 $iface 未处于 Monitor 模式" >&2
        echo "提示: 使用 'iwconfig $iface mode Monitor' 设置" >&2
        exit 1
    fi
    echo "检查通过: 网卡 $iface 处于 Monitor 模式"
}

# ============================================
# 设置网卡为 Monitor 模式
# ============================================
setup_monitor_mode() {
    local iface="$1"
    local band="$2"

    echo "正在配置网卡 $iface 为 Monitor 模式..."

    # 禁用 NetworkManager 管理
    nmcli device set "$iface" managed no 2>/dev/null || true

    # 关闭网卡
    ip link set "$iface" down

    # 设置 Monitor 模式
    iw dev "$iface" set monitor otherbss

    # 设置监管域（玻利维亚，允许最大功率）
    iw reg set BO

    # 启动网卡
    ip link set "$iface" up

    # 设置频道
    case "$band" in
        "5G")
            echo "设置 $iface 到频道 $CHANNEL5G"
            iw dev "$iface" set channel "$CHANNEL5G" HT20
            ;;
        "2G")
            echo "设置 $iface 到频道 $CHANNEL2G"
            iw dev "$iface" set channel "$CHANNEL2G" HT20
            ;;
        *)
            echo "错误: 请选择 2G 或 5G 频段" >&2
            exit 1
            ;;
    esac

    echo "检查通过: 网卡 $iface 已配置为 Monitor 模式"
}

# ============================================
# TUN 设备设置
# ============================================
setup_tun() {
    local tun_name="$1"
    local txqueuelen=100

    # 检查 TUN 设备是否已存在
    if ip link show "$tun_name" &>/dev/null; then
        echo "警告: TUN 设备 $tun_name 已存在，跳过创建"
    else
        # 创建 TUN 设备
        ip tuntap add dev "$tun_name" mode tun
        echo "TUN 设备 $tun_name 已创建"
    fi

    # 启动 TUN 设备
    ip link set "$tun_name" up

    # 设置 txqueuelen（关键：设置为 100 触发快速丢包反压）
    ip link set "$tun_name" txqueuelen $txqueuelen

    echo "TUN 设备 $tun_name 已配置，txqueuelen=$txqueuelen"
}

# ============================================
# 日志目录创建
# ============================================
setup_logging() {
    local log_dir
    log_dir=$(dirname "$LOG_FILE")

    if [[ ! -d "$log_dir" ]]; then
        mkdir -p "$log_dir"
        echo "日志目录已创建: $log_dir"
    else
        echo "日志目录已存在: $log_dir"
    fi
}

# ============================================
# 显示配置信息
# ============================================
print_config() {
    echo "============================================"
    echo "WFB-NG 服务器启动配置"
    echo "============================================"
    echo "无线网卡: $WLAN_IFACE"
    echo "MCS 索引: $MCS_INDEX"
    echo "频宽: ${BANDWIDTH}MHz"
    echo "节点数量: $NUM_NODES"
    echo "TUN 设备: $TUN_NAME"
    echo "日志文件: $LOG_FILE"
    echo "频段: $BAND"
    echo "============================================"
}

# ============================================
# 主程序启动
# ============================================
start_server() {
    echo "启动 WFB-NG 服务器..."
    echo "配置: MCS=$MCS_INDEX, 频宽=${BANDWIDTH}MHz, 节点数=$NUM_NODES"

    # 获取脚本所在目录
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

    # 检查可执行文件是否存在
    if [[ -x "$PROJECT_ROOT/wfb_core" ]]; then
        echo "启动主程序: $PROJECT_ROOT/wfb_core"
        "$PROJECT_ROOT/wfb_core" \
            --mcs "$MCS_INDEX" \
            --bandwidth "$BANDWIDTH" \
            --nodes "$NUM_NODES" \
            --iface "$WLAN_IFACE" \
            --tun "$TUN_NAME" \
            2>&1 | tee "$LOG_FILE"
    else
        echo "警告: 未找到 wfb_core 可执行文件" >&2
        echo "提示: 请先编译项目或检查可执行文件路径" >&2
        echo "预期路径: $PROJECT_ROOT/wfb_core"
        echo ""
        echo "请根据实际可执行文件路径调整启动命令"
        exit 1
    fi
}

# ============================================
# 清理函数
# ============================================
cleanup() {
    echo ""
    echo "正在清理..."

    # 删除 TUN 设备（可选）
    if ip link show "$TUN_NAME" &>/dev/null; then
        echo "提示: TUN 设备 $TUN_NAME 仍存在"
        echo "如需删除，请运行: ip tuntap del dev $TUN_NAME mode tun"
    fi

    echo "服务器已停止"
}

# 注册清理函数
trap cleanup EXIT INT TERM

# ============================================
# 主流程
# ============================================
main() {
    echo "============================================"
    echo "WFB-NG 服务器启动脚本"
    echo "============================================"
    echo ""

    # 1. 检查 Root 权限
    check_root

    # 2. 配置网卡（如果尚未处于 Monitor 模式）
    if ! iwconfig "$WLAN_IFACE" 2>&1 | grep -q "Mode:Monitor"; then
        setup_monitor_mode "$WLAN_IFACE" "$BAND"
    else
        check_monitor_mode "$WLAN_IFACE"
    fi

    # 3. 设置 TUN 设备
    setup_tun "$TUN_NAME"

    # 4. 设置日志目录
    setup_logging

    # 5. 显示配置信息
    print_config

    # 6. 启动服务器
    start_server
}

# 执行主流程
main "$@"
