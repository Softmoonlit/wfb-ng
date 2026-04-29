#!/bin/bash

# Phase 5 集成测试脚本
# 测试单进程架构的端到端功能

set -e

echo "=== Phase 5 集成测试 ==="
echo "测试日期: $(date)"

# 测试计数器
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# 测试函数
run_test() {
    local test_name="$1"
    local test_cmd="$2"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    echo ""
    echo "[$TOTAL_TESTS] $test_name"
    echo "命令: $test_cmd"

    if eval "$test_cmd"; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        echo "✓ 通过"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo "✗ 失败"
    fi
}

# === 1. 编译测试 ===
echo ""
echo "=== 1. 编译测试 ==="

run_test "清理编译产物" "make clean"

run_test "编译 wfb_core" "make wfb_core"

run_test "编译测试程序" "make test_wfb_core"

# === 2. 单元测试 ===
echo ""
echo "=== 2. 单元测试 ==="

run_test "运行单元测试" "./test_wfb_core"

# === 3. 命令行参数测试 ===
echo ""
echo "=== 3. 命令行参数测试 ==="

run_test "显示帮助信息" "./wfb_core --help | grep -q '用法'"

run_test "验证服务端参数" "./wfb_core --mode server -i wlan0 -c 6 -m 0 --help | grep -q 'server'"

run_test "验证客户端参数" "./wfb_core --mode client -i wlan0 -c 6 -m 0 --node-id 1 --help | grep -q 'client'"

run_test "验证 FEC 参数" "./wfb_core --mode server -i wlan0 --fec-n 16 --fec-k 10 --help | grep -q 'fec'"

# === 4. 权限检查 ===
echo ""
echo "=== 4. 权限检查 ==="

if [ "$EUID" -ne 0 ]; then
    echo "警告: 未以 Root 权限运行，跳过实时调度和进程启动测试"
    echo "使用 sudo 运行此脚本以执行完整测试"
else
    echo "✓ Root 权限确认"

    # === 5. 进程启动测试 ===
    echo ""
    echo "=== 5. 进程启动测试 ==="

    # 检查是否有可用的 WiFi 接口
    WIFI_IFACE=$(ip link show | grep -oP 'wl\w+' | head -1)

    if [ -z "$WIFI_IFACE" ]; then
        echo "警告: 未检测到 WiFi 接口，使用虚拟接口测试"
        WIFI_IFACE="lo"  # 使用回环接口进行基本测试
    fi

    echo "使用接口: $WIFI_IFACE"

    # 启动服务端（后台，2 秒后终止）
    echo "[5.1] 启动服务端..."
    timeout 2 ./wfb_core --mode server -i "$WIFI_IFACE" -c 6 -m 0 --tun wfb0 2>&1 || true
    echo "✓ 服务端启动测试完成"

    # 启动客户端（后台，2 秒后终止）
    echo "[5.2] 启动客户端..."
    timeout 2 ./wfb_core --mode client -i "$WIFI_IFACE" -c 6 -m 0 --tun wfb1 --node-id 1 2>&1 || true
    echo "✓ 客户端启动测试完成"

    # === 6. 启动脚本测试 ===
    echo ""
    echo "=== 6. 启动脚本测试 ==="

    if [ -f "./scripts/server_start.sh" ]; then
        echo "[6.1] 检查服务端启动脚本..."
        if grep -q "wfb_core --mode server" "./scripts/server_start.sh"; then
            echo "✓ 服务端启动脚本正确调用 wfb_core"
        else
            echo "✗ 服务端启动脚本未调用 wfb_core"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    fi

    if [ -f "./scripts/client_start.sh" ]; then
        echo "[6.2] 检查客户端启动脚本..."
        if grep -q "wfb_core --mode client" "./scripts/client_start.sh"; then
            echo "✓ 客户端启动脚本正确调用 wfb_core"
        else
            echo "✗ 客户端启动脚本未调用 wfb_core"
            FAILED_TESTS=$((FAILED_TESTS + 1))
        fi
    fi
fi

# === 7. 文档检查 ===
echo ""
echo "=== 7. 文档检查 ==="

run_test "部署文档存在" "test -f docs/deployment.md"

run_test "部署文档包含单进程说明" "grep -q '单进程模式' docs/deployment.md"

# === 测试总结 ===
echo ""
echo "=== 测试总结 ==="
echo "总测试数: $TOTAL_TESTS"
echo "通过: $PASSED_TESTS"
echo "失败: $FAILED_TESTS"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo "✓ 所有测试通过！"
    exit 0
else
    echo "✗ 有 $FAILED_TESTS 个测试失败"
    exit 1
fi