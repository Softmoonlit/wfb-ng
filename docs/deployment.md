# wfb-ng 部署指南（单进程版本）

## 概述

本文档描述 wfb-ng 单进程架构的部署方法。单进程架构将控制面、数据面和发射端整合在单个 `wfb_core` 进程中，消除了多进程通信的开销和丢包风险。

## 系统要求

### 硬件要求
- WiFi 网卡：支持 Monitor 模式（推荐 RTL8812AU）
- CPU：双核及以上
- 内存：512MB 以上

### 软件要求
- 操作系统：Linux（内核 3.10+）
- 依赖库：libpcap, libzfex
- 权限：Root（实时调度需要）

## 单进程模式（推荐）

### 服务端启动

```bash
# 基本启动
sudo ./scripts/server_start.sh -i wlan0 -c 6 -m 0

# 指定 FEC 参数
sudo ./scripts/server_start.sh -i wlan0 -c 6 -m 0 --fec-n 16 --fec-k 10

# 详细日志
sudo ./scripts/server_start.sh -i wlan0 -c 6 -m 0 -v
```

### 客户端启动

```bash
# 基本启动（节点 ID 1）
sudo ./scripts/client_start.sh -i wlan0 -c 6 -m 0 --node-id 1

# 指定 FEC 参数
sudo ./scripts/client_start.sh -i wlan0 -c 6 -m 0 --node-id 1 --fec-n 16 --fec-k 10

# 详细日志
sudo ./scripts/client_start.sh -i wlan0 -c 6 -m 0 --node-id 1 -v
```

## 参数说明

### 通用参数

| 参数 | 说明 | 默认值 | 范围 |
|------|------|--------|------|
| `-i` | WiFi 网卡接口 | wlan0 | 系统接口名 |
| `-c` | 信道号 | 6 | 1-14 |
| `-m` | MCS 调制方案 | 0 | 0-8 |
| `--bw` | 频宽（MHz） | 20 | 20, 40 |
| `--tun` | TUN 设备名 | wfb0 | 任意名称 |
| `--fec-n` | FEC 总块数 | 12 | 2-255 |
| `--fec-k` | FEC 数据块数 | 8 | 1-254 |
| `-v` | 详细日志 | 关闭 | - |
| `--legacy` | 旧多进程模式 | 关闭 | - |

### 客户端专用参数

| 参数 | 说明 | 默认值 | 范围 |
|------|------|--------|------|
| `--node-id` | 节点 ID | 必需 | 1-255 |

### 单进程模式优势

1. **降低延迟**: 线程间共享内存通信，消除 UDP 套接字开销
2. **零碰撞保证**: 服务端中心化调度，彻底避免射频冲突
3. **动态窗口调节**: 基于 MCS 速率的智能水位线计算
4. **实时调度**: 关键线程 SCHED_FIFO 优先级，保证时序确定性
5. **快速反压**: TUN 队列深度 100，秒级排队延迟上限

### 启动模式对比

| 模式 | 进程数 | 通信方式 | 延迟 | 可靠性 | 推荐度 |
|------|--------|----------|------|--------|--------|
| **单进程模式** | 1 | 线程间共享内存 | 极低 | 极高 | ★★★★★ |
| 旧多进程模式 | 3 | UDP 本地回环 | 较高 | 中等 | ★★☆☆☆ |

## 权限要求

必须以 Root 权限运行（SCHED_FIFO 实时调度需要）：

```bash
# 检查权限
whoami  # 应为 root

# 使用 sudo
sudo ./scripts/server_start.sh ...
```

## 性能优化

### CPU 亲和性

绑定 wfb_core 到专用 CPU 核心，避免核心切换：

```bash
# 绑定到 CPU 1
taskset -c 1 ./build/wfb_core --mode server ...
```

### CPU 频率模式

设置为性能模式，避免频率波动：

```bash
# 设置所有核心为性能模式
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > "$cpu"
done
```

### 实时调度验证

```bash
# 检查实时调度是否生效
ps -eLo pid,tid,class,rtprio,comm | grep wfb_core

# 应显示 FIFO 和优先级（99, 95, 90）
```

## 故障排查

### 实时调度失败

```bash
# 检查内核实时调度支持
cat /proc/sys/kernel/sched_rt_runtime_us

# 如果值为 -1，表示实时调度被禁用
# 启用实时调度
echo -1 > /proc/sys/kernel/sched_rt_runtime_us
```

### TUN 设备创建失败

```bash
# 检查 TUN 模块
lsmod | grep tun

# 手动加载
modprobe tun

# 检查 /dev/net/tun 权限
ls -la /dev/net/tun
```

### WiFi Monitor 模式设置失败

```bash
# 检查网卡是否支持 Monitor 模式
iw list | grep "Monitor"

# 手动设置
ip link set wlan0 down
iw dev wlan0 set type monitor
ip link set wlan0 up
```

### 日志分析

```bash
# 查看日志
tail -f /var/log/wfb_core_server.log

# 检查统计信息
# 服务端应输出：
# - Token 发送数
# - Token 失败数
# - 空闲巡逻次数
# - 节点切换次数
```

## 架构说明

### 单进程架构

```
wfb_core 进程
├── RX 线程 (SCHED_FIFO 99) - 接收 WiFi 数据，解析 Token
├── 调度器线程 (SCHED_FIFO 95) - 服务端 Token 分配（仅 Server 模式）
├── TX 线程 (SCHED_FIFO 90) - 零轮询唤醒，门控发射
└── TUN 读取线程 (CFS) - TUN 设备读取，FEC 编码
```

### 共享状态

所有线程通过 `ThreadSharedState` 共享数据：

- `can_send`: Token 授权标志
- `token_expire_time_ms`: Token 过期时间
- `packet_queue`: 环形队列（容量 10000）
- `cv_send`: 条件变量（零轮询唤醒）

## 从多进程迁移

如果您正在从旧的多进程架构迁移：

1. 停止旧进程：

```bash
sudo killall wfb_tx wfb_rx wfb_tun
```

2. 使用新的启动脚本：

```bash
# 服务端
sudo ./scripts/server_start.sh -i wlan0 -c 6 -m 0

# 客户端
sudo ./scripts/client_start.sh -i wlan0 -c 6 -m 0 --node-id 1
```

3. 如需向后兼容，使用 `--legacy` 参数（不推荐）

## 监控与告警

### 关键指标

- **Token 发送率**: 应接近 100%
- **数据包发送率**: 应 > 95%
- **Token 过期次数**: 应 < 5%
- **反压事件**: 表示队列拥塞

### 监控命令

```bash
# 实时查看统计
watch -n 1 'tail -20 /var/log/wfb_core_server.log | grep "统计"'

# 检查进程状态
ps aux | grep wfb_core
```