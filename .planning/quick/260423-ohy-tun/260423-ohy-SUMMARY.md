---
status: complete
quick_id: 260423-ohy
completed: 2026-04-23T09:38:30Z
---

# Summary: 记录 TUN 路由冲突问题及解决方案

## Completed Tasks

### 1. 问题诊断

**现象**：
- client（虚拟机）ping server（本机）：正常
- server ping client：100% 丢包

**排查过程**：
```bash
# 在 server 上检查路由
ip route get 10.5.0.2 from 10.5.0.1
# 输出：local 10.5.0.2 from 10.5.0.1 dev lo table local
# 问题：走的是 loopback，不是 TUN

# 检查 TUN 设备
ip addr show | grep -E '10.5|tun'
# 发现：tun0: 10.5.0.1/24 (UP), tun1: 10.5.0.2/24 (DOWN)
```

**根因**：
- 本机存在旧的 `tun1` 设备，地址与虚拟机 client 相同（10.5.0.2）
- 系统路由表认为 10.5.0.2 是本地地址
- 所有发往 10.5.0.2 的包走 local 路由，不进入 TUN 设备

### 2. 解决方案

```bash
sudo ip tuntap del dev tun1 mode tun
```

验证：
```bash
ip route show | grep 10.5
# 10.5.0.0/24 dev tun0 proto kernel scope link src 10.5.0.1

ip route get 10.5.0.2 from 10.5.0.1
# 10.5.0.2 from 10.5.0.1 dev tun0
```

### 3. 预防措施

**测试前检查清单**：
```bash
# 1. 检查残留 TUN 设备
ip link show | grep tun

# 2. 如有残留，清理
sudo ip tuntap del dev tunX mode tun

# 3. 确保网段唯一
ip addr show | grep 10.5
```

## Key Lessons

1. **TUN 设备地址冲突**：同一网段内不能有多个 TUN 设备使用相同地址
2. **路由优先级**：local 路由优先于 TUN 路由
3. **测试环境隔离**：本机测试和虚拟机测试的 TUN 地址要分开规划

## Files Updated

无（本任务为问题记录，未修改代码文件）
