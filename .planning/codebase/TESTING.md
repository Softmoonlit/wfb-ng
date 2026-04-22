# 测试策略

**分析日期:** 2026-04-22

## 测试框架与机制

**基本策略:**
- 使用 C++ 标准库中的 `<cassert>` 宏 `assert()` 来进行轻量级单元测试，不依赖第三方复杂框架（如 GoogleTest, Catch2）。
- 典型的断言形式为 `assert(condition);`，如果条件为假会触发程序中止。

**测试文件组织:**
- 测试文件存放在 `tests/` 目录下。
- 文件命名以 `test_` 开头，如 `test_mac_token.cpp`, `test_packet_queue.cpp`, `test_scheduler.cpp`。

## 测试结构与模式

**断言模式:**
```cpp
void test_token_serialization() {
    TokenFrame original{0x02, 5, 25, 1001};
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(original, buffer);
    TokenFrame parsed = parse_token(buffer);

    // 验证序列化和反序列化是否正确
    assert(parsed.magic == 0x02);
    assert(parsed.target_node == 5);
}

int main() {
    test_token_serialization();
    return 0;
}
```

**执行方式:**
- 直接在 `main` 函数中按顺序调用独立的测试函数。若中途任一断言失败，可执行程序会直接报错退出，提示具体行号。

---

*测试分析日期: 2026-04-22*
