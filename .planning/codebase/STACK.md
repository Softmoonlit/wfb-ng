# Technology Stack

**Analysis Date:** 2026-04-22

## Languages

**Primary:**
- C++11 - Core networking code, transceivers (`src/tx.cpp`, `src/rx.cpp`)
- C99 - Utility tools and library compatible code (`src/zfex.c`, `src/wfb_tun.c`)

**Secondary:**
- Python 3 - Build tools, setup, configuration management (`setup.py`, `version.py`)
- Bash - Build wrapper and system scripts (`Makefile`)

## Runtime

**Environment:**
- Linux (Requires standard POSIX / Linux environment)
- C++11 standard library

**Package Manager:**
- Python pip (for virtualenv in build)
- system package manager (apt/dpkg via `stdeb`)
- Lockfile: missing

## Frameworks

**Core:**
- Standard Library - C++ STL containers and types

**Testing:**
- Catch2 v3 - C++ unit testing framework (`src/fec_test.cpp`, `src/libsodium_test.cpp`)
- twisted.trial - Python unit test framework

**Build/Dev:**
- GNU Make - Primary build tool
- pkg-config - Dependency discovery
- Python virtualenv - Isolated build environments

## Key Dependencies

**Critical:**
- libpcap - Packet capture and injection for networking code
- libsodium - Cryptography library for secure communication
- libevent_core - Event notification loop (`src/wfb_tun.c`)
- gstreamer-rtsp-server-1.0 - Video streaming (`src/rtsp_server.c`)

**Infrastructure:**
- Python packages (for system management): `pyroute2`, `pyserial`, `msgpack`, `jinja2`, `yaml`, `twisted`
- System tools: `socat`, `iw`

## Configuration

**Environment:**
- Makefile environment variables (`ARCH`, `PYTHON`, `OS_CODENAME`)

**Build:**
- `Makefile` - Contains compiler definitions (`-std=gnu++11`, `-std=gnu99`, optimization flags)
- Architecture-specific macros (`-DZFEX_USE_INTEL_SSSE3`, `-DZFEX_USE_ARM_NEON`)

## Platform Requirements

**Development:**
- GCC/G++ with C++11 support
- Make, Python 3, pkg-config
- Development headers for libpcap, libsodium, libevent

**Production:**
- Linux (specifically targeted for Raspberry Pi, Debian-based systems via stdeb/rpm builders)
- Tun/tap kernel support
- 802.11 Monitor mode capable hardware

---

*Stack analysis: 2026-04-22*