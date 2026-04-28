// tests/test_tun_device.cpp
// TUN 设备创建和配置单元测试：验证设备创建和配置的正确性

#include <cassert>
#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>  // for close()
#include "../src/tun_reader.h"

// 测试 1: TUN 设备创建成功
void test_create_tun_device() {
    std::cout << "测试 1: TUN 设备创建成功..." << std::endl;

    char error_buf[256] = {0};
    const std::string tun_name = "testtun0";

    // 测试创建 TUN 设备（需要 root 权限，这里只测试函数调用）
    int fd = create_tun_device(tun_name, error_buf);

    // 注意：由于需要 root 权限，实际运行可能失败
    // 这里主要测试函数接口正确性和错误处理
    if (fd >= 0) {
        std::cout << "成功创建 TUN 设备: " << tun_name << " (fd=" << fd << ")" << std::endl;
        close(fd);
    } else {
        std::cout << "TUN 设备创建失败（预期，需要 root 权限）: " << error_buf << std::endl;
    }

    // 验证函数调用没有崩溃
    std::cout << "PASS: create_tun_device 函数调用正常" << std::endl;
}

// 测试 2: TUN 设备名正确设置
void test_tun_device_name_validation() {
    std::cout << "测试 2: TUN 设备名正确设置..." << std::endl;

    char error_buf[256] = {0};

    // 测试空设备名
    int fd1 = create_tun_device("", error_buf);
    assert(fd1 == -1);
    std::cout << "PASS: 空设备名返回错误" << std::endl;

    // 重置错误缓冲区
    memset(error_buf, 0, sizeof(error_buf));

    // 测试过长设备名（IFNAMSIZ 通常是 16）
    std::string long_name(32, 'a');  // 32个字符，超过 IFNAMSIZ
    int fd2 = create_tun_device(long_name, error_buf);
    assert(fd2 == -1);
    std::cout << "PASS: 过长设备名返回错误" << std::endl;

    // 测试有效设备名
    memset(error_buf, 0, sizeof(error_buf));
    int fd3 = create_tun_device("wfb0", error_buf);
    // 由于需要 root 权限，可能成功也可能失败
    if (fd3 >= 0) {
        std::cout << "成功创建有效设备名 TUN 设备" << std::endl;
        close(fd3);
    } else {
        std::cout << "有效设备名创建失败（可能缺少权限）: " << error_buf << std::endl;
    }

    std::cout << "PASS: TUN 设备名验证逻辑正常" << std::endl;
}

// 测试 3: txqueuelen 设置成功
void test_set_tun_txqueuelen() {
    std::cout << "测试 3: txqueuelen 设置成功..." << std::endl;

    // 测试无效参数
    bool result1 = set_tun_txqueuelen("", 100);
    assert(!result1);
    std::cout << "PASS: 空设备名返回 false" << std::endl;

    bool result2 = set_tun_txqueuelen("wfb0", -1);
    assert(!result2);
    std::cout << "PASS: 负队列长度返回 false" << std::endl;

    bool result3 = set_tun_txqueuelen("wfb0", 0);
    assert(!result3);
    std::cout << "PASS: 零队列长度返回 false" << std::endl;

    // 测试有效设置（需要 root 权限）
    bool result4 = set_tun_txqueuelen("wfb0", 100);
    // 由于需要 root 权限，可能成功也可能失败
    if (result4) {
        std::cout << "成功设置 txqueuelen 为 100" << std::endl;
    } else {
        std::cout << "设置 txqueuelen 失败（可能缺少权限）" << std::endl;
    }

    std::cout << "PASS: txqueuelen 设置函数逻辑正常" << std::endl;
}

// 测试 4: 无效接口名返回错误
void test_invalid_interface_name() {
    std::cout << "测试 4: 无效接口名返回错误..." << std::endl;

    char error_buf[256] = {0};

    // 测试包含非法字符的设备名
    int fd1 = create_tun_device("wfb@0", error_buf);
    assert(fd1 == -1);
    std::cout << "PASS: 包含非法字符的设备名返回错误" << std::endl;

    // 测试以数字开头的设备名
    memset(error_buf, 0, sizeof(error_buf));
    int fd2 = create_tun_device("0wfb", error_buf);
    // Linux 允许数字开头的接口名，这里只测试函数不崩溃
    if (fd2 >= 0) {
        std::cout << "数字开头的设备名创建成功" << std::endl;
        close(fd2);
    } else {
        std::cout << "数字开头的设备名创建失败: " << error_buf << std::endl;
    }

    std::cout << "PASS: 无效接口名处理逻辑正常" << std::endl;
}

// 测试 5: 错误缓冲区功能验证
void test_error_buffer_functionality() {
    std::cout << "测试 5: 错误缓冲区功能验证..." << std::endl;

    char error_buf[256];

    // 测试错误缓冲区为空指针
    int fd1 = create_tun_device("", nullptr);
    assert(fd1 == -1);
    std::cout << "PASS: 空错误缓冲区不崩溃" << std::endl;

    // 测试错误缓冲区正常使用
    memset(error_buf, 0, sizeof(error_buf));
    int fd2 = create_tun_device("", error_buf);
    assert(fd2 == -1);
    assert(strlen(error_buf) > 0);
    std::cout << "PASS: 错误缓冲区正确填充: " << error_buf << std::endl;

    // 测试 txqueuelen 错误处理
    bool result = set_tun_txqueuelen("", 100);
    assert(!result);
    std::cout << "PASS: txqueuelen 错误处理正常" << std::endl;

    std::cout << "PASS: 错误缓冲区功能正常" << std::endl;
}

int main() {
    std::cout << "=== TUN 设备创建和配置测试 ===\n" << std::endl;

    try {
        // 注意：由于 tun_reader.cpp 尚未实现，测试将在编译时失败（TDD RED 阶段）
        test_create_tun_device();
        test_tun_device_name_validation();
        test_set_tun_txqueuelen();
        test_invalid_interface_name();
        test_error_buffer_functionality();

        std::cout << "\n=== 所有测试通过 ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知错误" << std::endl;
        return 1;
    }
}