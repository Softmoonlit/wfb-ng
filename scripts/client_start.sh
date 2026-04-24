#!/bin/bash
# WFB-NG 客户端启动脚本
# 包含系统检查、配置加载、TUN 设备设置
# 实现 APP-03, APP-04 需求
set -e

# ============================================
# 配置参数
# ============================================
WLAN_IFACE="${WLAN_IFACE:-wlan0}"           # 无线网卡接口
MCS_INDEX="${MCS_INDEX:-6}"                  # MCS 索引（0-8）
BANDWIDTH="${BANDWIDTH:-20}"                 # 频宽（MHz）
TUN_NAME="${TUN_NAME:-tun1}"                 # TUN 设备名称
TUN_ADDR="${TUN_ADDR:-10.5.0.2/24}"           # TUN 设备地址（客户端侧）
SERVER_IP="${SERVER_IP:-192.168.100.1}"      # 服务器 IP
LOG_FILE="${LOG_FILE:-/var/log/wfb-ng/client.log}"  # 日志文件路径
BAND="${BAND:-5G}"                           # 频段（2G/5G）
CHANNEL2G="${CHANNEL2G:-6}"                  # 2G 频道
CHANNEL5G="${CHANNEL5G:-149}"                # 5G 频道
NODE_ID="${NODE_ID:-1}"                      # 客户端节点 ID

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
# 路由配置
# ============================================
setup_routing() {
    local tun_name="$1"
    local server_ip="$2"

    # 将服务器 IP 路由到 TUN 设备
    if ip route show | grep -q "$server_ip dev $tun_name"; then
        echo "警告: 路由 $server_ip -> $tun_name 已存在"
    else
        ip route add "$server_ip" dev "$tun_name"
        echo "路由已配置: $server_ip -> $tun_name"
    fi
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
    echo "WFB-NG 客户端启动配置"
    echo "============================================"
    echo "无线网卡: $WLAN_IFACE"
    echo "MCS 索引: $MCS_INDEX"
    echo "频宽: ${BANDWIDTH}MHz"
    echo "服务器 IP: $SERVER_IP"
    echo "节点 ID: $NODE_ID"
    echo "TUN 设备: $TUN_NAME"
    echo "日志文件: $LOG_FILE"
    echo "频段: $BAND"
    echo "============================================"
}

# ============================================
# 主程序启动
# ============================================
start_client() {
    echo "启动 WFB-NG 客户端..."
    echo "配置: MCS=$MCS_INDEX, 频宽=${BANDWIDTH}MHz, 服务器=$SERVER_IP"

    # 获取脚本所在目录
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

    # UDP 端口配置
    TX_UDP_PORT="${TX_UDP_PORT:-5600}"
    RX_UDP_PORT="${RX_UDP_PORT:-5800}"

    # 检查可执行文件
    local wfb_tx="$PROJECT_ROOT/wfb_tx"
    local wfb_rx="$PROJECT_ROOT/wfb_rx"
    local wfb_tun="$PROJECT_ROOT/wfb_tun"

    if [[ ! -x "$wfb_tx" ]]; then
        echo "错误: 未找到 wfb_tx 可执行文件" >&2
        echo "预期路径: $wfb_tx" >&2
        exit 1
    fi

    if [[ ! -x "$wfb_rx" ]]; then
        echo "错误: 未找到 wfb_rx 可执行文件" >&2
        echo "预期路径: $wfb_rx" >&2
        exit 1
    fi

    if [[ ! -x "$wfb_tun" ]]; then
        echo "错误: 未找到 wfb_tun 可执行文件" >&2
        echo "预期路径: $wfb_tun" >&2
        exit 1
    fi

    echo "使用独立进程模式："
    echo "  - wfb_tx: 无线发射端（从 UDP $TX_UDP_PORT 读取数据）"
    echo "  - wfb_rx: 无线接收端（转发到 UDP $RX_UDP_PORT）"
    echo "  - wfb_tun: TUN 设备桥接（$TX_UDP_PORT -> 无线, 无线 -> $RX_UDP_PORT）"
    echo ""

    # 启动 wfb_tx（后台运行）
    echo "启动 wfb_tx..."
    "$wfb_tx"         -K "$PROJECT_ROOT/drone.key"         -M "$MCS_INDEX"         -B "$BANDWIDTH"         -u "$TX_UDP_PORT"         -p 1         "$WLAN_IFACE"         >> "$LOG_FILE" 2>&1 &
    tx_pid=$!
    echo "wfb_tx 已启动 (PID: $tx_pid)"

    # 启动 wfb_rx（后台运行）
    echo "启动 wfb_rx..."
    "$wfb_rx"         -K "$PROJECT_ROOT/gs.key"         -p 0         -u "$RX_UDP_PORT"         "$WLAN_IFACE"         >> "$LOG_FILE" 2>&1 &
    rx_pid=$!
    echo "wfb_rx 已启动 (PID: $rx_pid)"

    # 等待进程初始化
    sleep 1

    # 启动 wfb_tun（前台运行）
    echo "启动 wfb_tun..."
    echo "按 Ctrl+C 停止客户端"
    echo ""

    ip addr flush dev "$TUN_NAME" 2>/dev/null || true

    "$wfb_tun"         -u "$TX_UDP_PORT"         -t "$TUN_NAME"         -a "$TUN_ADDR"         -l "$RX_UDP_PORT"         2>&1 | tee -a "$LOG_FILE"
}

# ============================================
# 清理函数
# ============================================
cleanup() {
    echo ""
    echo "正在清理..."

    # 停止后台进程
    if [[ -n "${tx_pid:-}" ]] && kill -0 "$tx_pid" 2>/dev/null; then
        echo "停止 wfb_tx (PID: $tx_pid)..."
        kill "$tx_pid" 2>/dev/null || true
        wait "$tx_pid" 2>/dev/null || true
    fi

    if [[ -n "${rx_pid:-}" ]] && kill -0 "$rx_pid" 2>/dev/null; then
        echo "停止 wfb_rx (PID: $rx_pid)..."
        kill "$rx_pid" 2>/dev/null || true
        wait "$rx_pid" 2>/dev/null || true
    fi

    # 删除 TUN 设备（可选）
    if ip link show "$TUN_NAME" &>/dev/null; then
        echo "提示: TUN 设备 $TUN_NAME 仍存在"
        echo "如需删除，请运行: ip tuntap del dev $TUN_NAME mode tun"
    fi

    echo "客户端已停止"
}

# 注册清理函数
trap cleanup EXIT INT TERM

# ============================================
# 主流程
# ============================================
main() {
    echo "============================================"
    echo "WFB-NG 客户端启动脚本"
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

    # 4. 设置路由
    setup_routing "$TUN_NAME" "$SERVER_IP"

    # 5. 设置日志目录
    setup_logging

    # 6. 显示配置信息
    print_config

    # 7. 启动客户端
    start_client
}

# 执行主流程
main "$@"
