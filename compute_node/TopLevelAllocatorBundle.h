#ifndef REMON_TOP_LEVEL_ALLOCATOR_BUNDLE_H
#define REMON_TOP_LEVEL_ALLOCATOR_BUNDLE_H

#include "TopLevelAllocator.h"
#include "log.h"
#include "SpinLock.h"

class TopLevelAllocatorBundle : public log {
private:
    unordered_map<thread::id, TopLevelAllocator*> allocatorMap;
    vector<TopLevelAllocator*> allocatedAllocators;
    SpinLock muxLock;
    size_t lastAllocated = 0;
    size_t maxArena = 0;

    vector<pod *>* podsRef;
    vector<pod*>& pods;
    size_t localPodCount;
    size_t globalPodCount;
    void* base;
    size_t pageSize;

    size_t threadAdd = 0;
    size_t loopAdd = 0;

public:
    TopLevelAllocatorBundle(vector<pod *> *podsPtr, size_t podCount, void *basePtr, size_t pageSize);
    ~TopLevelAllocatorBundle();
    TopLevelAllocator* getTopLevelAllocator();

    void crossOwnershipHugeFree(pod *pPod);

    pod *allocateNPages(size_t size);

    TopLevelAllocator *addNewRegion();

    atomic_bool lockLoopOn {false};

    void reportStatistics();

    TopLevelAllocator *perThreadGet();

    TopLevelAllocator *loopGet();

    void trim();

    TopLevelAllocator *addNewRegionCore();
};


#endif //REMON_TOP_LEVEL_ALLOCATOR_BUNDLE_H
