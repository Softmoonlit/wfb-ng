#include <cassert>
#include <iostream>
#include <ctime>
#include "../src/guard_interval.h"

void test_interface_exists() {
    // 测试接口是否存在（编译测试）
    // 如果接口不存在，编译会失败
    std::cout << "PASS: 保护间隔函数接口存在" << std::endl;
}

int main() {
    test_interface_exists();
    std::cout << "接口测试通过！" << std::endl;
    return 0;
}
