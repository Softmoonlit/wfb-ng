# 外部集成

**分析日期:** 2026-04-22

## 核心系统依赖

**网络数据包捕获与注入:**
- `libpcap` - 核心报文嗅探和发送工具，使用于 `src/rx.cpp` 等
  - 依赖项：`<pcap.h>`，使用 BPF 过滤器(`bpf_program`)，混杂模式(`pcap_set_promisc`)。

**加密与安全:**
- `libsodium` - 使用于会话密钥协商、流加密等 (`crypto_box_SECRETKEYBYTES`, `<sodium.h>`)
  - 测试套件中包含 `src/libsodium_test.cpp` 以验证其工作。

**无线网络通信:**
- `Radiotap 头` - 使用自定义的 radiotap 头处理用于 802.11 监控模式下的帧封装 (`src/ieee80211_radiotap.h`)。
- Linux 802.11 支持与网络接口控制 (通过 `ioctl`, `<linux/if.h>`)。

## 外部库集成 (C/C++)

**多媒体与流:**
- `gstreamer-rtsp-server-1.0` - RTSP 服务器集成 (`src/rtsp_server.c`)，允许在无线链路上直接传输视频流。
  - 使用: `pkg-config --cflags/libs gstreamer-rtsp-server-1.0`

**事件驱动:**
- `libevent` - 核心事件循环，用于 `src/wfb_tun.c` 等。
  - 依赖项: `<event.h>`, 链接参数 `-levent_core`。

## Python 依赖

**系统级和工具类库:**
- `pyroute2`, `pyserial`, `msgpack`, `jinja2`, `yaml`, `twisted`
  - 这些库主要用于网络配置、监控面板生成或系统服务，定义于 RPM/DEB 的打包规范中 (`setup.py`)。

---

*集成分析日期: 2026-04-22*