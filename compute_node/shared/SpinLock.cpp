#include "SpinLock.h"

void SpinLock::lock(string reason){
#ifdef DEBUG_SPIN_LOCK
    size_t count = 0;
#endif
    while (spinlock.test_and_set(std::memory_order_acquire)) {
#ifdef DEBUG_SPIN_LOCK
        count++;
#endif
    }
#ifdef DEBUG_SPIN_LOCK
    if(count > 0) {
        stringstream ss;
        debug(ss << name << " spin lock count too high: " << count << " reason: " << prevReason);
    }
#endif
}

void SpinLock::unlock(string reason) {
    spinlock.clear(std::memory_order_release);
#ifdef DEBUG_SPIN_LOCK
    prevReason = reason;
#endif
}

SpinLock::SpinLock(string name) : log("spin_lock") {
    this->name = name;
}

bool SpinLock::tryLock(string reason) {
    if (spinlock.test_and_set(std::memory_order_acquire)) {
        return false;
    }
    return true;
}
