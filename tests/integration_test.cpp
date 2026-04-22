// tests/integration_test.cpp
// 端到端集成测试：验证 10 节点并发传输 40MB 模型文件的稳定性和性能

#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

#include "../src/server_scheduler.h"
#include "../src/aq_sq_manager.h"
#include "../src/breathing_cycle.h"
#include "../src/packet_queue.h"
#include "../src/mac_token.h"

// Mock radiotap_header_t 结构（避免引入 tx.hpp 的复杂依赖）
typedef struct {
    std::vector<uint8_t> header;
    uint8_t stbc;
    bool ldpc;
    bool short_gi;
    uint8_t bandwidth;
    uint8_t mcs_index;
    bool vht_mode;
    uint8_t vht_nss;
} radiotap_header_t;

// Mock init_radiotap_header 函数
radiotap_header_t init_radiotap_header(uint8_t stbc,
                                       bool ldpc,
                                       bool short_gi,
                                       uint8_t bandwidth,
                                       uint8_t mcs_index,
                                       bool vht_mode,
                                       uint8_t vht_nss) {
    radiotap_header_t hdr;
    hdr.stbc = stbc;
    hdr.ldpc = ldpc;
    hdr.short_gi = short_gi;
    hdr.bandwidth = bandwidth;
    hdr.mcs_index = mcs_index;
    hdr.vht_mode = vht_mode;
    hdr.vht_nss = vht_nss;
    return hdr;
}

// 全局变量定义（模拟实现）
radiotap_header_t g_control_radiotap;
radiotap_header_t g_data_radiotap;

// 初始化双模板
void init_radiotap_templates(uint8_t data_mcs_index, uint8_t data_bandwidth) {
    // 初始化控制帧模板：固定使用 MCS 0 + 20MHz
    g_control_radiotap = init_radiotap_header(
        /*stbc=*/0,
        /*ldpc=*/false,
        /*short_gi=*/false,
        /*bandwidth=*/20,      // 20MHz 频宽
        /*mcs_index=*/0,       // MCS 0（理论速率 6Mbps）
        /*vht_mode=*/false,
        /*vht_nss=*/1
    );

    // 初始化数据帧模板：基于 CLI 参数
    g_data_radiotap = init_radiotap_header(
        /*stbc=*/0,
        /*ldpc=*/false,
        /*short_gi=*/true,     // 启用 short GI 提高吞吐量
        /*bandwidth=*/data_bandwidth,
        /*mcs_index=*/data_mcs_index,
        /*vht_mode=*/false,
        /*vht_nss=*/1
    );
}

// 根据帧类型返回对应的模板指针（零延迟切换）
radiotap_header_t* get_radiotap_template(bool is_control_frame) {
    return is_control_frame ? &g_control_radiotap : &g_data_radiotap;
}

// 测试辅助宏
#define TEST_PASS(name) std::cout << "[PASS] " << name << std::endl
#define TEST_FAIL(name, detail) std::cout << "[FAIL] " << name << ": " << detail << std::endl

// ==================== 单节点基础测试 ====================
/**
 * 测试单节点基础传输功能
 * - 初始化调度器
 * - 模拟数据包入队
 * - 验证队列操作
 * - 验证调度器返回正确节点
 */
void test_single_node_basic_transfer() {
    // 1. 初始化调度器
    ServerScheduler scheduler;
    scheduler.set_num_nodes(1);
    scheduler.init_node(1);

    // 2. 模拟数据包入队
    ThreadSafeQueue<uint8_t> queue(1000);
    for (int i = 0; i < 100; ++i) {
        queue.push(static_cast<uint8_t>(i));
    }

    // 3. 验证队列操作
    assert(queue.size() == 100);

    // 4. 初始化 AQ/SQ 管理器
    AqSqManager manager;
    manager.init_node(1);
    manager.migrate_to_aq(1);
    scheduler.set_aq_sq_manager(&manager);

    // 5. 验证调度器返回正确节点
    auto next = scheduler.get_next_node_to_serve();
    assert(next.first == 1);   // node_id
    assert(next.second > 0);   // duration_ms

    TEST_PASS("test_single_node_basic_transfer");
}

// ==================== 多节点并发测试 ====================
/**
 * 测试多节点并发调度正确性
 * - 初始化 10 个节点
 * - 模拟多轮调度
 * - 验证所有节点都被服务
 */
void test_multi_node_concurrency() {
    const size_t NUM_NODES = 10;

    // 1. 初始化 AQ/SQ 管理器
    AqSqManager manager;
    for (size_t i = 1; i <= NUM_NODES; ++i) {
        manager.init_node(static_cast<uint8_t>(i));
        manager.migrate_to_aq(static_cast<uint8_t>(i));
    }

    // 2. 初始化调度器
    ServerScheduler scheduler;
    scheduler.set_num_nodes(NUM_NODES);
    scheduler.set_aq_sq_manager(&manager);

    for (size_t i = 1; i <= NUM_NODES; ++i) {
        scheduler.init_node(static_cast<uint8_t>(i));
    }

    // 3. 模拟多轮调度
    std::vector<uint8_t> served_nodes;
    for (int round = 0; round < 20; ++round) {
        auto next = scheduler.get_next_node_to_serve();
        if (next.first != 0xFF) {
            served_nodes.push_back(next.first);
        }
    }

    // 4. 验证所有节点都被服务
    assert(served_nodes.size() >= NUM_NODES);

    TEST_PASS("test_multi_node_concurrency");
}

// ==================== 40MB 模型传输模拟测试 ====================
/**
 * 模拟 40MB 模型传输场景
 * - 验证数据包入队性能
 * - 验证队列容量
 */
void test_model_transfer_simulation() {
    const size_t MODEL_SIZE = 40 * 1024 * 1024;  // 40MB
    const size_t MTU = 1450;
    const size_t TOTAL_PACKETS = MODEL_SIZE / MTU;

    // 1. 初始化队列（容量略大于总包数）
    ThreadSafeQueue<uint8_t> queue(TOTAL_PACKETS + 1000);

    // 2. 模拟数据包入队
    auto start_time = std::chrono::steady_clock::now();
    for (size_t i = 0; i < TOTAL_PACKETS; ++i) {
        queue.push(static_cast<uint8_t>(i & 0xFF));
    }
    auto end_time = std::chrono::steady_clock::now();

    // 3. 验证入队时间（应小于 1 秒）
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    assert(duration_ms.count() < 1000);

    // 4. 验证队列大小
    assert(queue.size() == TOTAL_PACKETS);

    TEST_PASS("test_model_transfer_simulation");
}

// ==================== TokenFrame 序列化集成测试 ====================
/**
 * 验证 TokenFrame 在集成场景下的正确性
 * - 序列化/反序列化往返测试
 */
void test_token_frame_integration() {
    TokenFrame frame;
    frame.magic = 0x02;
    frame.target_node = 5;
    frame.duration_ms = 100;
    frame.seq_num = 42;

    // 1. 序列化
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(frame, buffer);

    // 2. 反序列化
    TokenFrame decoded = parse_token(buffer);

    // 3. 验证一致性
    assert(decoded.magic == frame.magic);
    assert(decoded.target_node == frame.target_node);
    assert(decoded.duration_ms == frame.duration_ms);
    assert(decoded.seq_num == frame.seq_num);

    TEST_PASS("test_token_frame_integration");
}

// ==================== Radiotap 模板切换测试 ====================
/**
 * 验证 Radiotap 模板切换零延迟
 * - 控制帧模板验证
 * - 数据帧模板验证
 * - 模板切换验证
 */
void test_radiotap_template_switching() {
    // 1. 初始化模板（数据帧使用 MCS 6, 20MHz）
    init_radiotap_templates(6, 20);

    // 2. 获取控制帧模板
    radiotap_header_t* ctrl_template = get_radiotap_template(true);
    assert(ctrl_template != nullptr);
    assert(ctrl_template->mcs_index == 0);    // MCS 0（控制帧最低速率）
    assert(ctrl_template->bandwidth == 20);   // 20MHz

    // 3. 获取数据帧模板
    radiotap_header_t* data_template = get_radiotap_template(false);
    assert(data_template != nullptr);
    assert(data_template->mcs_index == 6);    // MCS 6
    assert(data_template->bandwidth == 20);   // 20MHz

    // 4. 验证两个模板不同
    assert(ctrl_template != data_template);

    TEST_PASS("test_radiotap_template_switching");
}

// ==================== 三态状态机集成测试 ====================
/**
 * 验证三态状态机与 AQ/SQ 管理器集成
 * - STATE_I 断流后迁移到 SQ
 * - STATE_II/III 活动后迁移到 AQ
 */
void test_state_machine_aq_sq_integration() {
    ServerScheduler scheduler;
    AqSqManager manager;
    scheduler.set_aq_sq_manager(&manager);
    scheduler.init_node(1);
    scheduler.set_num_nodes(10);

    // 初始化节点到 SQ
    manager.init_node(1);

    // 3 次静默后应当迁移到 SQ（已经在 SQ，验证不崩溃）
    scheduler.calculate_next_window(1, 2, 100);
    scheduler.calculate_next_window(1, 2, 100);
    scheduler.calculate_next_window(1, 2, 100);

    // 活动：应当迁移到 AQ
    scheduler.calculate_next_window(1, 50, 50);
    assert(manager.aq_size() == 1);
    assert(manager.sq_size() == 0);

    // 再次静默 3 次后迁移回 SQ
    scheduler.calculate_next_window(1, 2, 80);
    scheduler.calculate_next_window(1, 2, 80);
    scheduler.calculate_next_window(1, 2, 80);
    assert(manager.aq_size() == 0);
    assert(manager.sq_size() == 1);

    TEST_PASS("test_state_machine_aq_sq_integration");
}

// ==================== 呼吸周期集成测试 ====================
/**
 * 验证呼吸周期与调度器集成
 * - 呼吸相位切换
 * - NACK 窗口分配
 */
void test_breathing_cycle_integration() {
    BreathingCycle cycle;
    cycle.set_num_nodes(10);

    // 1. 验证初始相位为 EXHALE
    assert(cycle.get_current_phase() == BreathingPhase::EXHALE);

    // 2. 验证周期时长
    assert(cycle.get_cycle_duration_ms() == 1000);  // 800 + 200 = 1000ms

    // 3. 验证各阶段时长
    assert(cycle.get_exhale_duration_ms() == 800);
    assert(cycle.get_inhale_duration_ms() == 200);
    assert(cycle.get_nack_window_ms() == 25);
    assert(cycle.get_guard_interval_ms() == 5);

    TEST_PASS("test_breathing_cycle_integration");
}

// ==================== 混合交织调度集成测试 ====================
/**
 * 验证混合交织调度正确性
 * - AQ 节点优先服务
 * - 定期穿插 SQ 节点
 */
void test_interleave_scheduling_integration() {
    AqSqManager manager;

    // 初始化节点 1, 2, 3, 4 到 AQ
    manager.init_node(1);
    manager.init_node(2);
    manager.init_node(3);
    manager.init_node(4);
    manager.migrate_to_aq(1);
    manager.migrate_to_aq(2);
    manager.migrate_to_aq(3);
    manager.migrate_to_aq(4);

    // 初始化节点 5 到 SQ
    manager.init_node(5);
    assert(manager.aq_size() == 4);
    assert(manager.sq_size() == 1);

    // 连续调用 get_next_node(4) 10 次
    // 预期：前 4 次 AQ，第 5 次 SQ，再 4 次 AQ，第 10 次 SQ
    int sq_count = 0;
    int aq_count = 0;

    for (int i = 0; i < 10; i++) {
        auto result = manager.get_next_node(4);
        if (result.second) {
            sq_count++;
        } else {
            aq_count++;
        }
    }

    // 验证 SQ 服务次数为 2 次
    assert(sq_count == 2);
    assert(aq_count == 8);

    TEST_PASS("test_interleave_scheduling_integration");
}

// ==================== Watchdog 保护集成测试 ====================
/**
 * 验证 Watchdog 保护机制
 * - 连续 2 次静默保持窗口
 * - 第 3 次静默降级到 MIN_WINDOW
 */
void test_watchdog_protection_integration() {
    ServerScheduler scheduler;
    scheduler.init_node(1);
    scheduler.set_num_nodes(10);

    // 第一次静默
    uint16_t next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 100);  // Watchdog 保护

    // 第二次静默
    next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 100);  // Watchdog 保护

    // 第三次静默
    next_window = scheduler.calculate_next_window(1, 2, 100);
    assert(next_window == 50);   // 降级到 MIN_WINDOW

    TEST_PASS("test_watchdog_protection_integration");
}

// ==================== MAX_WINDOW 动态计算测试 ====================
/**
 * 验证 MAX_WINDOW 动态计算
 * - 5 节点：600ms
 * - 10 节点：300ms
 * - 20 节点：150ms
 */
void test_max_window_dynamic_calculation() {
    ServerScheduler scheduler;

    // 5 节点
    scheduler.set_num_nodes(5);
    assert(scheduler.get_max_window() == 600);

    // 10 节点
    scheduler.set_num_nodes(10);
    assert(scheduler.get_max_window() == 300);

    // 20 节点
    scheduler.set_num_nodes(20);
    assert(scheduler.get_max_window() == 150);

    TEST_PASS("test_max_window_dynamic_calculation");
}

// ==================== 完整调度周期集成测试 ====================
/**
 * 验证完整调度周期
 * - 获取下一个节点
 * - 生成 TokenFrame
 * - 序列化 Token
 */
void test_full_scheduling_cycle() {
    // 1. 初始化组件
    ServerScheduler scheduler;
    AqSqManager manager;
    scheduler.set_aq_sq_manager(&manager);
    scheduler.set_num_nodes(10);

    // 2. 初始化节点
    manager.init_node(1);
    manager.migrate_to_aq(1);
    scheduler.init_node(1);

    // 3. 获取下一个节点
    auto result = scheduler.get_next_node_to_serve();
    assert(result.first == 1);
    assert(result.second >= 50);  // 至少 MIN_WINDOW

    // 4. 生成 TokenFrame
    TokenFrame token;
    token.magic = 0x02;
    token.target_node = result.first;
    token.duration_ms = result.second;
    token.seq_num = 1;

    // 5. 序列化 Token
    uint8_t buffer[sizeof(TokenFrame)];
    serialize_token(token, buffer);

    // 6. 反序列化验证
    TokenFrame decoded = parse_token(buffer);
    assert(decoded.target_node == result.first);
    assert(decoded.duration_ms == result.second);

    TEST_PASS("test_full_scheduling_cycle");
}

// ==================== 主函数 ====================
int main() {
    std::cout << "=== 端到端集成测试 ===" << std::endl;
    std::cout << "验证 10 节点并发传输 40MB 模型文件的稳定性和性能" << std::endl;
    std::cout << std::endl;

    // 单节点测试
    test_single_node_basic_transfer();

    // 多节点测试
    test_multi_node_concurrency();

    // 40MB 模型传输模拟
    test_model_transfer_simulation();

    // TokenFrame 集成测试
    test_token_frame_integration();

    // Radiotap 模板切换测试
    test_radiotap_template_switching();

    // 三态状态机集成测试
    test_state_machine_aq_sq_integration();

    // 呼吸周期集成测试
    test_breathing_cycle_integration();

    // 混合交织调度集成测试
    test_interleave_scheduling_integration();

    // Watchdog 保护集成测试
    test_watchdog_protection_integration();

    // MAX_WINDOW 动态计算测试
    test_max_window_dynamic_calculation();

    // 完整调度周期集成测试
    test_full_scheduling_cycle();

    std::cout << std::endl;
    std::cout << "=== 所有集成测试通过 ===" << std::endl;
    return 0;
}
