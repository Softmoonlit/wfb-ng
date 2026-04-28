#!/bin/bash

# wfb-ng 客户端启动脚本（单进程版本）
# 支持 --legacy 参数使用旧的多进程模式

set -e

# === 配置区 ===
WFB_CORE_BIN="./build/wfb_core"
PID_FILE="/var/run/wfb_core_client.pid"
LOG_FILE="/var/log/wfb_core_client.log"

# 默认参数
WIFI_INTERFACE="wlan0"
CHANNEL=6
MCS=0
BANDWIDTH=20
TUN_NAME="wfb0"
NODE_ID=1
VERBOSE=false
LEGACY_MODE=false

# === 辅助函数 ===

log_info() {
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') $1"
}

log_warn() {
    echo "[WARN] $(date '+%Y-%m-%d %H:%M:%S') $1" >&2
}

log_error() {
    echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') $1" >&2
}

log_success() {
    echo "[SUCCESS] $(date '+%Y-%m-%d %H:%M:%S') $1"
}

print_usage() {
    cat << EOF
用法: $0 [选项]

选项:
    -i <接口>       WiFi 网卡接口（默认: wlan0）
    -c <信道>       信道号（默认: 6）
    -m <MCS>        MCS 调制方案 0-8（默认: 0）
    --bw <MHz>      频宽 20/40（默认: 20）
    --tun <名称>    TUN 设备名（默认: wfb0）
    --node-id <ID>  节点 ID 1-255（默认: 1，必需）
    --fec-n <N>     FEC 总块数（默认: 12）
    --fec-k <K>     FEC 数据块数（默认: 8）
    --legacy        使用旧的多进程模式
    -v              详细日志
    -h, --help      显示帮助

示例:
    # 单进程模式（推荐）
    sudo $0 -i wlan0 -c 6 -m 0 --node-id 1

    # 旧多进程模式（向后兼容）
    sudo $0 -i wlan0 -c 6 -m 0 --node-id 1 --legacy
EOF
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        log_error "必须以 Root 权限运行"
        exit 1
    fi
}

check_interface() {
    if ! ip link show "$1" &>/dev/null; then
        log_error "WiFi 接口不存在: $1"
        exit 1
    fi
    log_info "WiFi 接口已确认: $1"
}

set_monitor_mode() {
    log_info "设置 Monitor 模式: $1"
    ip link set "$1" down
    iw dev "$1" set type monitor
    ip link set "$1" up
    iw dev "$1" set channel "$2"
    log_success "Monitor 模式设置成功"
}

cleanup() {
    log_info "收到退出信号，清理资源..."

    if [ -n "$WFB_PID" ] && kill -0 "$WFB_PID" 2>/dev/null; then
        log_info "发送 SIGTERM 到 wfb_core (PID: $WFB_PID)..."
        kill -TERM "$WFB_PID"
        wait "$WFB_PID" 2>/dev/null || true
    fi

    # 删除 TUN 设备
    if ip link show "$TUN_NAME" &>/dev/null; then
        ip link delete "$TUN_NAME" 2>/dev/null || true
    fi

    # 删除 PID 文件
    rm -f "$PID_FILE"

    log_success "清理完成"
    exit 0
}

# === 解析参数 ===

while [[ $# -gt 0 ]]; do
    case $1 in
        -i) WIFI_INTERFACE="$2"; shift 2 ;;
        -c) CHANNEL="$2"; shift 2 ;;
        -m) MCS="$2"; shift 2 ;;
        --bw) BANDWIDTH="$2"; shift 2 ;;
        --tun) TUN_NAME="$2"; shift 2 ;;
        --node-id) NODE_ID="$2"; shift 2 ;;
        --fec-n) FEC_N="$2"; shift 2 ;;
        --fec-k) FEC_K="$2"; shift 2 ;;
        --legacy) LEGACY_MODE=true; shift ;;
        -v) VERBOSE=true; shift ;;
        -h|--help) print_usage; exit 0 ;;
        *) log_error "未知参数: $1"; print_usage; exit 1 ;;
    esac
done

# 验证节点 ID
if [ -z "$NODE_ID" ] || [ "$NODE_ID" -lt 1 ] || [ "$NODE_ID" -gt 255 ]; then
    log_error "节点 ID 必须在 1-255 范围内"
    exit 1
fi

# === 主流程 ===

log_info "启动 wfb-ng 客户端 (节点 ID: $NODE_ID)..."
check_root
check_interface "$WIFI_INTERFACE"

# 设置信号处理
trap cleanup SIGINT SIGTERM

# 设置 Monitor 模式
set_monitor_mode "$WIFI_INTERFACE" "$CHANNEL"

# 检查启动模式
if [ "$LEGACY_MODE" = true ]; then
    log_warn "使用旧的多进程模式（不推荐）"
    log_error "旧模式未实现，请使用单进程模式"
    exit 1
else
    # === 单进程模式 ===
    log_info "启动 wfb_core 单进程..."

    # 检查可执行文件
    if [ ! -f "$WFB_CORE_BIN" ]; then
        log_warn "wfb_core 未找到，尝试编译..."
        make clean && make
    fi

    # 构建命令行
    CMD="$WFB_CORE_BIN --mode client -i $WIFI_INTERFACE -c $CHANNEL -m $MCS --bw $BANDWIDTH --tun $TUN_NAME --node-id $NODE_ID"

    if [ -n "$FEC_N" ]; then
        CMD="$CMD --fec-n $FEC_N"
    fi
    if [ -n "$FEC_K" ]; then
        CMD="$CMD --fec-k $FEC_K"
    fi
    if [ "$VERBOSE" = true ]; then
        CMD="$CMD -v"
    fi

    # 启动 wfb_core
    log_info "执行: $CMD"
    $CMD 2>&1 | tee "$LOG_FILE" &
    WFB_PID=$!

    # 保存 PID
    echo "$WFB_PID" > "$PID_FILE"
    log_success "wfb_core 已启动 (PID: $WFB_PID, 节点 ID: $NODE_ID)"

    # 等待进程退出
    wait "$WFB_PID"
    EXIT_CODE=$?

    log_info "wfb_core 已退出 (退出码: $EXIT_CODE)"
fi

# 清理
cleanup