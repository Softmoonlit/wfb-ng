// tests/test_packet_queue.cpp
#include <cassert>
#include <stdexcept>
#include "../src/packet_queue.h"

struct DummyPacket { int id; };

void test_queue() {
    ThreadSafeQueue<DummyPacket> queue(10000);
    assert(queue.empty());

    DummyPacket p1{1};
    queue.push(p1);
    assert(queue.size() == 1);

    DummyPacket popped = queue.pop();
    assert(popped.id == 1);
    assert(queue.empty());
}

int main() {
    test_queue();
    return 0;
}
