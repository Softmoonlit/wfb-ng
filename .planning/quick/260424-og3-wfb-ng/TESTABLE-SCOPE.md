# WFB-NG 可测试范围指南

**版本**: v1.0  
**更新日期**: 2026-04-24  
**适用阶段**: Phase 1-3 完成，Phase 4 待开始

---

## 执行摘要

当前系统已完成底层核心架构（Phase 1-3），**可以进行单元测试和集成测试**，但**不建议进行大规模多对一 UFTP 压力测试**，因为 Phase 4（UFTP 集成与优化）尚未开始。

### 当前状态速览

| 测试类型 | 状态 | 风险等级 |
|---------|------|---------|
| 单元测试 | 可执行 | 无 |
| 集成测试（模拟环境） | 可执行 | 低 |
| 单节点连通性测试 | 可执行 | 低 |
| 多节点小规模测试（2-3节点） | 可执行 | 中 |
| 大规模多对一 UFTP 测试（10节点） | **不建议** | 高 |

---

## 一、现在可以进行的测试

### 1.1 单元测试（全部可用）

所有单元测试已通过验证，可直接运行：

```bash
# Token 帧单元测试 - 验证 Token 帧序列化/反序列化
./test_mac_token

# 环形队列单元测试 - 验证线程安全队列操作
./test_packet_queue

# 动态水位线单元测试 - 验证 MCS 映射表计算
./test_watermark

# 调度器单元测试 - 验证三态状态机跃迁
./test_scheduler

# AQ/SQ 队列管理测试 - 验证授权/调度队列
./test_aq_sq

# 保护间隔测试 - 验证 5ms 高精度定时
./test_guard_interval
```

**预期输出**:
```
=== 测试名称 ===
[PASS] 测试用例1
[PASS] 测试用例2
...
所有测试通过！
```

### 1.2 集成测试（模拟环境）

集成测试在模拟环境中验证端到端流程，**无需真实无线网卡**：

```bash
# 运行完整集成测试套件
./integration_test
```

**测试覆盖内容**:
- 单节点基础传输
- 多节点并发调度
- 40MB 模型文件传输模拟
- Token 帧集成
- Radiotap 模板切换
- 三态状态机与 AQ/SQ 集成
- 下行呼吸周期
- 混合交织调度
- Watchdog 保护机制
- 动态 MAX_WINDOW 计算
- 完整调度周期

**预期输出**:
```
=== 端到端集成测试 ===
[PASS] test_single_node_basic_transfer
[PASS] test_multi_node_concurrency
[PASS] test_model_transfer_simulation
[PASS] test_token_frame_integration
[PASS] test_radiotap_template_switching
[PASS] test_state_machine_aq_sq_integration
[PASS] test_breathing_cycle_integration
[PASS] test_interleave_scheduling_integration
[PASS] test_watchdog_protection_integration
[PASS] test_max_window_dynamic_calculation
[PASS] test_full_scheduling_cycle

=== 应用层集成验证测试 ===
[PASS] test_tun_device_interception
[PASS] test_control_frame_mechanism
[PASS] test_zero_perception_validation
[PASS] test_end_to_end_transfer_validation

=== 所有集成测试通过 ===
```

### 1.3 单进程架构测试

验证 wfb_core 单进程入口可正常启动：

```bash
# 检查 wfb_core 帮助信息
./wfb_core --help

# 验证 wfb_core 可执行（需要 root 权限）
sudo ./wfb_core --mode server --num-nodes 2 --wlan wlan0 --dry-run
```

### 1.4 单节点连通性测试（需要真实硬件）

**环境要求**:
- 2 台设备（1 服务端 + 1 客户端）
- RTL8812AU 网卡或兼容网卡
- Root 权限

**测试步骤**:

```bash
# ========== 服务端 ==========
# 1. 设置网卡为 Monitor 模式
sudo ip link set wlan0 down
sudo iw dev wlan0 set monitor otherbss
sudo iw reg set BO
sudo ip link set wlan0 up
sudo iw dev wlan0 set channel 149

# 2. 启动服务端（单节点模式）
sudo ./wfb_core --mode server --num-nodes 1 --wlan wlan0 --mcs 6 --bandwidth 20

# ========== 客户端 ==========
# 1. 设置网卡为 Monitor 模式（同上）

# 2. 启动客户端
sudo ./wfb_core --mode client --node-id 1 --wlan wlan0 --server-ip 192.168.100.1
```

**验证方法**:
- 观察控制帧收发日志
- 检查 Token 帧是否正确传递
- 验证 TUN 设备创建成功（ip link show tun0）

### 1.5 多节点小规模测试（2-3 节点）

在确认单节点连通后，可进行小规模多节点测试：

```bash
# 服务端启动（3 节点模式）
sudo ./wfb_core --mode server --num-nodes 3 --wlan wlan0 --mcs 6 --bandwidth 20

# 客户端 1 启动
sudo ./wfb_core --mode client --node-id 1 --wlan wlan0 --server-ip 192.168.100.1

# 客户端 2 启动
sudo ./wfb_core --mode client --node-id 2 --wlan wlan0 --server-ip 192.168.100.1
```

**注意事项**:
- 此阶段主要用于验证调度器多节点逻辑
- 不建议传输大文件，仅建议 ping 测试
- 观察各节点是否按 Token 顺序获得发送权

---

## 二、不建议进行的测试

### 2.1 大规模多对一 UFTP 测试（10 节点，40MB 模型）

**状态**: 不建议  
**原因**: Phase 4 尚未完成，以下功能未就绪：

| 缺失功能 | 影响 |
|---------|------|
| UFTP 限速配置指南未完成 | 无法正确设置 `-R` 参数，可能导致缓冲区溢出 |
| 控制帧模板分流集成未完成 | 控制帧可能使用错误 MCS，导致响应延迟过高 |
| MCS 映射表集成未完成 | 水位线计算可能不准确 |
| 心跳干扰问题未解决 | wfb_tun 心跳可能干扰 Token 调度 |
| 冷启动延迟问题未解决 | 无线链路初始化可能导致首次传输失败 |

**风险**:
- UFTP 超时断连
- 数据包丢失率高
- 频谱碰撞（如果保护间隔未正确生效）
- 节点间调度冲突

**建议等待**: Phase 4 完成后，按官方集成指南进行测试

### 2.2 长时间压力测试（> 1 小时）

**状态**: 不建议  
**原因**:
- 内存泄漏检测未完整验证
- 长时间运行下的调度器稳定性未验证
- 时钟漂移对保护间隔的影响未评估

### 2.3 多频段混合测试（2.4G + 5G）

**状态**: 不建议  
**原因**:
- 频段切换逻辑未完整实现
- 跨频段干扰模式未验证

---

## 三、测试环境准备

### 3.1 硬件要求

| 组件 | 最低要求 | 推荐配置 |
|------|---------|---------|
| 无线网卡 | RTL8812AU 或兼容芯片 | RTL8812AU x 2 |
| 处理器 | ARM Cortex-A53 / x86_64 | x86_64 多核 |
| 内存 | 512MB | 1GB+ |
| 操作系统 | Linux 5.4+ | Ubuntu 22.04 / Raspberry Pi OS |

### 3.2 软件依赖

```bash
# 安装依赖
sudo apt-get update
sudo apt-get install -y iw wireless-tools tcpdump libpcap-dev

# 编译项目
make clean && make
```

### 3.3 网卡初始化脚本

```bash
#!/bin/bash
# setup_monitor.sh - 设置 Monitor 模式
IFACE=${1:-wlan0}
CHANNEL=${2:-149}

sudo ip link set $IFACE down
sudo iw dev $IFACE set monitor otherbss
sudo iw reg set BO
sudo ip link set $IFACE up
sudo iw dev $IFACE set channel $CHANNEL

echo "网卡 $IFACE 已设置为 Monitor 模式，频道 $CHANNEL"
```

---

## 四、测试命令速查表

### 快速验证命令

```bash
# 一键运行所有单元测试
make test

# 仅运行集成测试
./integration_test

# 检查所有测试二进制是否存在
ls -la test_* wfb_core 2>/dev/null
```

### 服务端启动命令

```bash
# 开发测试模式（单节点）
sudo ./wfb_core --mode server --num-nodes 1 --wlan wlan0 --dry-run

# 3 节点测试模式
sudo ./wfb_core --mode server --num-nodes 3 --wlan wlan0 --mcs 6 --bandwidth 20

# 生产模式（10 节点，待 Phase 4 完成后使用）
sudo ./wfb_core --mode server --num-nodes 10 --wlan wlan0 --mcs 6 --bandwidth 20
```

### 客户端启动命令

```bash
# 客户端模式（指定节点 ID）
sudo ./wfb_core --mode client --node-id 1 --wlan wlan0 --server-ip 192.168.100.1
```

### 调试命令

```bash
# 抓包分析控制帧
sudo tcpdump -i wlan0 -w /tmp/wfb_capture.pcap

# 查看 TUN 设备状态
ip link show tun0
ip addr show tun0

# 查看无线网卡状态
iw dev wlan0 info
iwconfig wlan0
```

---

## 五、问题排查

### 5.1 单元测试失败

```bash
# 重新编译测试
make clean
make test

# 单独运行失败测试获取详细输出
./test_scheduler --verbose
```

### 5.2 权限问题

```bash
# 确保以 root 运行
sudo -i
# 或
sudo ./wfb_core ...

# 检查 capabilities（可选）
sudo setcap cap_net_admin,cap_net_raw+eip ./wfb_core
```

### 5.3 网卡无法进入 Monitor 模式

```bash
# 检查网卡驱动
ethtool -i wlan0

# 尝试卸载冲突驱动
sudo modprobe -r rtl88xxau  # 如果有冲突

# 使用 airmon-ng
sudo airmon-ng check kill
sudo airmon-ng start wlan0
```

---

## 六、下一步建议

1. **立即可以做**: 运行所有单元测试和集成测试，验证代码正确性
2. **本周可以做**: 准备硬件环境，进行单节点连通性测试
3. **等待 Phase 4**: 大规模 UFTP 多对一压力测试（10 节点，40MB 模型）

---

## 参考文档

- `.planning/STATE.md` - 项目当前状态
- `.planning/ROADMAP.md` - 阶段规划
- `CLAUDE.md` - 架构规范与陷阱防范
- `scripts/server_start.sh` - 服务端启动脚本
- `scripts/client_start.sh` - 客户端启动脚本
