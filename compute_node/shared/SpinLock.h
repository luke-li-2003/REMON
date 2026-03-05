#ifndef REMON_SPIN_LOCK_H
#define REMON_SPIN_LOCK_H

#include "atomic"
#include "string"
#include "iostream"
#include "log.h"

// #define DEBUG_SPIN_LOCK
using namespace std;
class SpinLock : public log {
private:
    string name;
    std::atomic_flag spinlock = ATOMIC_FLAG_INIT;
#ifdef DEBUG_SPIN_LOCK
    string prevReason;
#endif
public:
    explicit SpinLock(string name);
    void lock(string reason);
    void unlock(string reason);
    bool tryLock(string reason);
};


#endif //REMON_SPIN_LOCK_H
