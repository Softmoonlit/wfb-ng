---
created: 2026-04-23T08:17:12.319Z
title: 禁用 wfb_tun 心跳机制避免干扰 token 调度
area: core
files:
  - src/wfb_tun.c:38
---

## Problem

`wfb_tun` 每 500ms 自动发送心跳包（`PING_INTERVAL_MS`），这在原始 wfb-ng 中用于保持 NAT 映射和链路存活。

但在当前架构中：
- server 会频繁发送 token 帧来分配时隙
- token 帧已经承担了"保持链路活跃"的角色
- wfb_tun 的心跳包是**无调度的自发包**，不经过 token-passing
- 会在非分配时隙占用空口，可能干扰 server 的调度

## Solution

**方案 1（简单）：** 直接禁用心跳
```c
// src/wfb_tun.c:38
#define PING_INTERVAL_MS 0  // 禁用心跳
```

**方案 2（灵活）：** 添加命令行参数控制
```c
// 在 main() 中添加参数解析
int ping_interval_ms = 500;  // 默认值
// 允许通过 --ping-interval 0 禁用
```

启动脚本中：
```bash
wfb_tun ... --ping-interval 0  # 禁用心跳
```

**推荐方案 1**，因为 token-passing 架构下不需要额外心跳。
