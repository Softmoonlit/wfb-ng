// tests/test_packet_queue.cpp
#include <cassert>
#include <stdexcept>
#include "../src/packet_queue.h"

struct DummyPacket { int id; };

void test_queue() {
    ThreadSafeQueue<DummyPacket> queue(2);
    assert(queue.empty());
    assert(queue.size() == 0);

    DummyPacket p1{1};
    DummyPacket p2{2};
    DummyPacket p3{3};

    queue.push(p1);
    queue.push(p2);
    queue.push(p3);

    assert(queue.size() == 2);
    assert(!queue.empty());

    DummyPacket popped1 = queue.pop();
    DummyPacket popped2 = queue.pop();

    assert(popped1.id == 1);
    assert(popped2.id == 2);
    assert(queue.empty());
    assert(queue.size() == 0);
}

int main() {
    test_queue();
    return 0;
}
