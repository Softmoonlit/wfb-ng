#include <cassert>
#include "../src/threads.h"

void test_shared_state_defaults() {
    ThreadSharedState shared_state;
    assert(shared_state.can_send == false);
    assert(shared_state.token_expire_time_ms == 0);
}

void test_priority_constants() {
    assert(kControlThreadPriority == 99);
    assert(kTxThreadPriority == 90);
    assert(kGuardIntervalMs == 5);
}

int main() {
    test_shared_state_defaults();
    test_priority_constants();
    return 0;
}
