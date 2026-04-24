#!/bin/bash
echo "=== 编译单元测试 ==="
TESTS="test_mac_token test_packet_queue test_scheduler test_aq_sq test_breathing test_guard_interval"
FAILED=""

for test in $TESTS; do
    echo -n "编译 $test... "
    if g++ -Wall -O2 -std=gnu++11 -o $test tests/$test.cpp src/*.cpp -lpthread 2>&1 | grep -q "error"; then
        echo "失败"
        FAILED="$FAILED $test"
    else
        echo "成功"
    fi
done

echo ""
echo "=== 编译完成 ==="
echo "成功: $(echo $TESTS | wc -w) -$(echo $FAILED | wc -w) = $(($(echo $TESTS | wc -w) - $(echo $FAILED | wc -w)))"
if [ -n "$FAILED" ]; then
    echo "失败: $FAILED"
fi
