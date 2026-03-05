#ifndef REMON_SPL_MUX_H
#define REMON_SPL_MUX_H

#include "log.h"
#include "SegregatedPageList.h"
#include "SPLBundle.h"
#include <unordered_map>
#include <shared_mutex>
#include "SpinLock.h"

using namespace std;
class SplMux : public log {
private:
    unordered_map<thread::id, SPLBundle*> splMap;
    SpinLock muxLock;
    size_t maxAllocationSize;
public:
    SplMux(size_t maxAllocationSize);
    ~SplMux();
    SPLBundle * splGet();

    void trim();
};

#endif //REMON_SPL_MUX_H
