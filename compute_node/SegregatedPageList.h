#ifndef REMON_SEGREGATED_PAGE_LIST_H
#define REMON_SEGREGATED_PAGE_LIST_H

#include <list>
#include <unordered_map>
#include <map>
#include <cmath>
#include <mutex>
#include <unordered_set>
#include <chrono>
#include <set>

#include "shared/define.h"
#include "shared/pod.h"
#include "shared/profiler.h"
#include "shared/log.h"
#include "SpinLock.h"

class SegregatedPageList: public log {
    struct decendingOp {
        bool operator() (const int& a, const int& b) const {
            return a > b;
        }
    };
    map<size_t, unordered_set<pod*>*, decendingOp> rankMap;
    SpinLock lock;

    // helper functions internal to SPL
    void *allocateOfAList(size_t size, std::map<size_t, unordered_set<pod *> *>::iterator mapIt);
    void addPodFromSwappedState(pod *newPod);
    bool removePod(pod * podToRemove);
    unordered_set<pod*> localFreeBuffer;
    pod* lastAllocatedPod = nullptr;



public:
    SegregatedPageList();
    ~SegregatedPageList();

    // core functions
    void *allocate(size_t i);
    void* addPodAndAllocate(pod* newPod, size_t size);
    bool free(pod *podPtr, void *addr);

    // swap related
    bool benchSwappedPage(pod* podToBench);
    void unbenchSwappedPage(pod* podToUnbench);

    // debug
    void dumpDebug();
    string lockReason;
    void splGlobalLock(string reason) {
        lock.lock(reason);
    };
    void splGlobalUnlock(string reason) {
        lock.unlock(reason);
    };

    void clearLocalFreeBuffer();

    void flushOnePage(pod *pPod);

    void *tryAllocateWithPreviousPod(size_t size);

    void adjustPodList(pod *pPod);

    void trim();
};


#endif //REMON_SEGREGATED_PAGE_LIST_H
