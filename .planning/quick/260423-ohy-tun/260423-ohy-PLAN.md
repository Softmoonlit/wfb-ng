---
created: 2026-04-23T09:38:21.857Z
quick_id: 260423-ohy
mode: quick
---

# Quick Task: 记录 TUN 路由冲突问题及解决方案

## Task

在项目文档中记录 TUN 设备路由冲突问题及其解决方案，防止未来再次遇到相同错误。

## Problem

在 server（本机）和 client（虚拟机）双向测试时，发现：

1. **现象**：client → server ping 正常，但 server → client 无回包
2. **排查**：server 上 `ip route get 10.5.0.2 from 10.5.0.1` 显示走 `lo`（loopback）
3. **根因**：本机上存在旧的 `tun1` 设备（10.5.0.2/24），导致系统认为 10.5.0.2 是本地地址

## Solution

删除旧的 TUN 设备：
```bash
sudo ip tuntap del dev tun1 mode tun
```

验证路由正确：
```bash
ip route show | grep 10.5
# 应只显示：10.5.0.0/24 dev tun0 ...

ip route get 10.5.0.2 from 10.5.0.1
# 应显示走 tun0，而不是 lo
```

## Prevention

**测试环境配置检查清单**：
1. 启动脚本前，检查是否存在旧的 TUN 设备：`ip link show | grep tun`
2. 如有残留，先清理：`sudo ip tuntap del dev <tun_name> mode tun`
3. 确保每台机器只有一个 TUN 设备在 10.5.0.0/24 网段

## Files to Update

- `docs/TROUBLESHOOTING.md`（新建或追加）
