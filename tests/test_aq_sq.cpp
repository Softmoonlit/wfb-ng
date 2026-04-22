// tests/test_aq_sq.cpp
// AQ/SQ 队列管理器单元测试

#include <cassert>
#include <iostream>

#include "../src/aq_sq_manager.h"

void test_init_node() {
    AqSqManager manager;

    // 初始化节点 1, 2, 3
    manager.init_node(1);
    manager.init_node(2);
    manager.init_node(3);

    // 验证 SQ 大小为 3，AQ 为空
    assert(manager.sq_size() == 3);
    assert(manager.aq_size() == 0);
    assert(manager.is_aq_empty());

    std::cout << "[PASS] test_init_node" << std::endl;
}

void test_migrate_to_aq() {
    AqSqManager manager;

    // 初始化节点 1 到 SQ
    manager.init_node(1);
    assert(manager.sq_size() == 1);
    assert(manager.aq_size() == 0);

    // 迁移到 AQ
    manager.migrate_to_aq(1);
    assert(manager.aq_size() == 1);
    assert(manager.sq_size() == 0);

    std::cout << "[PASS] test_migrate_to_aq" << std::endl;
}

void test_migrate_to_sq() {
    AqSqManager manager;

    // 初始化节点 1 并迁移到 AQ
    manager.init_node(1);
    manager.migrate_to_aq(1);
    assert(manager.aq_size() == 1);
    assert(manager.sq_size() == 0);

    // 迁移回 SQ
    manager.migrate_to_sq(1);
    assert(manager.sq_size() == 1);
    assert(manager.aq_size() == 0);

    std::cout << "[PASS] test_migrate_to_sq" << std::endl;
}

void test_migrate_idempotent() {
    AqSqManager manager;

    // 初始化节点 1 并迁移到 AQ
    manager.init_node(1);
    manager.migrate_to_aq(1);
    assert(manager.aq_size() == 1);

    // 再次调用 migrate_to_aq，应保持不变（幂等性）
    manager.migrate_to_aq(1);
    assert(manager.aq_size() == 1);

    // 再次调用 migrate_to_sq 后再调用 migrate_to_sq
    manager.migrate_to_sq(1);
    assert(manager.sq_size() == 1);
    manager.migrate_to_sq(1);
    assert(manager.sq_size() == 1);

    std::cout << "[PASS] test_migrate_idempotent" << std::endl;
}

void test_get_next_node_empty() {
    AqSqManager manager;

    // 空 manager
    auto result = manager.get_next_node(4);

    // 应返回无效节点
    assert(result.first == AqSqManager::INVALID_NODE);
    assert(result.second == false);

    std::cout << "[PASS] test_get_next_node_empty" << std::endl;
}

void test_get_next_node_idle_patrol() {
    AqSqManager manager;

    // 初始化节点 1, 2 到 SQ，AQ 为空
    manager.init_node(1);
    manager.init_node(2);
    assert(manager.sq_size() == 2);
    assert(manager.is_aq_empty());

    // 调用 get_next_node
    auto result = manager.get_next_node(4);

    // 应从 SQ 获取节点
    assert(result.second == true);  // is_from_sq
    assert(result.first == 1 || result.first == 2);  // 节点 ID

    // 再次调用，验证轮询（下一个节点）
    auto result2 = manager.get_next_node(4);
    assert(result2.second == true);

    std::cout << "[PASS] test_get_next_node_idle_patrol" << std::endl;
}

void test_get_next_node_interleave() {
    AqSqManager manager;

    // 初始化节点 1, 2 到 AQ
    manager.init_node(1);
    manager.init_node(2);
    manager.migrate_to_aq(1);
    manager.migrate_to_aq(2);
    assert(manager.aq_size() == 2);

    // 初始化节点 3 到 SQ
    manager.init_node(3);
    assert(manager.sq_size() == 1);

    // 连续调用 get_next_node(4) 5 次
    // 前 4 次应返回 AQ 节点（is_from_sq=false）
    // 第 5 次应返回 SQ 节点（is_from_sq=true）
    int sq_count = 0;
    int aq_count = 0;

    for (int i = 0; i < 5; i++) {
        auto result = manager.get_next_node(4);
        if (result.second) {
            sq_count++;
        } else {
            aq_count++;
        }
    }

    // 前 4 次 AQ，第 5 次 SQ
    assert(aq_count == 4);
    assert(sq_count == 1);

    std::cout << "[PASS] test_get_next_node_interleave" << std::endl;
}

void test_node_rotation() {
    AqSqManager manager;

    // 初始化节点 1, 2 到 AQ
    manager.init_node(1);
    manager.init_node(2);
    manager.migrate_to_aq(1);
    manager.migrate_to_aq(2);

    // 连续获取，验证轮询（取出队首，放回队尾）
    auto first = manager.get_next_node(100);  // 大比例确保只从 AQ
    auto second = manager.get_next_node(100);

    // 轮询应交替
    assert(first.first != second.first);

    std::cout << "[PASS] test_node_rotation" << std::endl;
}

int main() {
    std::cout << "=== AQ/SQ Manager Tests ===" << std::endl;

    test_init_node();
    test_migrate_to_aq();
    test_migrate_to_sq();
    test_migrate_idempotent();
    test_get_next_node_empty();
    test_get_next_node_idle_patrol();
    test_get_next_node_interleave();
    test_node_rotation();

    std::cout << "=== All tests passed ===" << std::endl;
    return 0;
}
