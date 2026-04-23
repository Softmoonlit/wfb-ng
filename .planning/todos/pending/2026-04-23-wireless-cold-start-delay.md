---
created: 2026-04-23T10:01:37.824Z
title: 解决无线链路冷启动延迟问题
area: network
files:
  - scripts/keepalive.sh（待创建）
---

## Problem

长时间不通信后，ping 前几包延迟极高，之后恢复正常：

```
64 bytes from 10.5.0.1: icmp_seq=1 ttl=64 time=3124 ms
64 bytes from 10.5.0.1: icmp_seq=2 ttl=64 time=2062 ms
64 bytes from 10.5.0.1: icmp_seq=3 ttl=64 time=1038 ms
64 bytes from 10.5.0.1: icmp_seq=4 ttl=64 time=14.1 ms  # 恢复正常
```

### 现象

- **server**：本机，运行 wfb_tx / wfb_rx / wfb_tun
- **client**：虚拟机，运行 wfb_tx / wfb_rx / wfb_tun
- 长时间（> 1 分钟）无数据传输后，首次通信延迟高达秒级
- 第 3-4 包后恢复正常（14ms 左右）

### 根因分析

1. **无线网卡驱动空闲状态**（最可能）
   - `rtl88xxau_wfb` 驱动在长时间空闲后可能进入低功耗状态
   - Monitor 模式下没有 beacon 帧维持链路活跃
   - 需要几个包来"唤醒"无线链路

2. **ARP 缓存过期**
   - 长时间不通信，ARP 表项过期
   - 第一个包需要 ARP 请求/响应
   - 但这通常只影响第一包，不会持续 3 包

3. **FEC 解码器预热**
   - `wfb_rx` 的 FEC 解码器可能需要接收足够的包才能稳定
   - 但这不应该导致秒级延迟

### 验证信息

```bash
# 网卡驱动
driver: rtl88xxau_wfb

# 省电模式已关闭
Power save: off

# TUN 设备正常
tun0: UP, txqueuelen=100
```

## Solution

### 方案 1：保持链路活跃（推荐）

创建心跳脚本，定期发送 ping 维持无线链路：

```bash
#!/bin/bash
# scripts/keepalive.sh
while true; do
    ping -c 1 -W 1 -I 10.5.0.1 10.5.0.2 > /dev/null 2>&1
    sleep 5
done
```

### 方案 2：调整驱动参数

检查并调整 `rtl88xxau_wfb` 驱动的空闲超时参数。

### 方案 3：应用层心跳

在 UFTP 配置中启用 keepalive 机制。

## Priority

P2 - 影响用户体验，但不阻塞核心功能

## Related

- Quick Task 260423-ohy: TUN 路由冲突问题
- Todo #1: 禁用 wfb_tun 心跳机制
