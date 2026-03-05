#ifndef REMON_TOP_LEVEL_ALLOCATOR_H
#define REMON_TOP_LEVEL_ALLOCATOR_H

#include "define.h"
#include "log.h"
#include "pod.h"
#include <map>
#include "SpinLock.h"

class TopLevelAllocator : public log {
private:
    map<size_t, pod*> hugePodMap;
    mutex allocateNPagesLock;

    vector<pod*>& pods;
    size_t podCount;
    void* base;
    size_t pageSize;

    size_t from;
    size_t to;

public:
    TopLevelAllocator(vector<pod *> *podsPtr, size_t podCount, void *basePtr, size_t pageSize, size_t iFrom,
                        size_t iTo);
    void initGlobalFreePodList(size_t from, size_t to);
    void dumpGlobalFreePodList(string reason);
    void hugePodMapAddEntry(pod *pPod, string reason);
    void hugePodMapRemoveEntry(pod *podOfInterest);
    void setFreeOnFooter(pod *pPod);
    pod *allocateNPages(int numPages);
    void mapForPodI(size_t footerI);
    pod * coalescing(pod *podOfInterest);

    void coalescingOnePodRight(pod *pPod);
    pod * coalescingOnePodLeft(pod* podOfInterest);
    void hugeFree(pod *pPod);
    void globalLock() {
        allocateNPagesLock.lock();
    }
    void globalUnlock() {
        allocateNPagesLock.unlock();
    }
    bool globalTrylock() {
        return allocateNPagesLock.try_lock();
    }

    size_t huge = 0;
    size_t small = 0;

    void trim();

    void trimFreeBlock(size_t startI, size_t endI);
};


#endif //REMON_TOP_LEVEL_ALLOCATOR_H
