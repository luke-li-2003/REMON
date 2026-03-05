#include "TopLevelAllocatorBundle.h"

TopLevelAllocatorBundle::TopLevelAllocatorBundle(vector<pod *> *podsPtr, size_t podCount, void *basePtr, size_t pageSize)
                :
                pods(*podsPtr),
                log("top_level_allocator_bundle"),muxLock("top_level_allocator_bundle") {
    podsRef = podsPtr;
    globalPodCount = pods.size();
    localPodCount = podCount;
    this->pageSize = pageSize;
    base = basePtr;
    maxArena = globalPodCount / localPodCount;
    allocatedAllocators.reserve(maxArena);
}

TopLevelAllocator *TopLevelAllocatorBundle::getTopLevelAllocator() {
    if(!lockLoopOn) {
        TopLevelAllocator* result = perThreadGet();
        if(result == nullptr) {
            return loopGet();
        }
        return result;
    } else {
        return loopGet();
    }
}

TopLevelAllocatorBundle::~TopLevelAllocatorBundle() {
    reportStatistics();
    for(auto i : allocatedAllocators) {
        delete i;
    }
    stringstream ss;
    size_t allocateCount = lastAllocated / localPodCount;
    info(ss << "top level allocated: " << allocateCount << " times");
    info(ss << "thread_add: " << threadAdd << " loop_add: " << loopAdd);
}

void TopLevelAllocatorBundle::crossOwnershipHugeFree(pod *POI) {
    size_t region = POI->getI() / localPodCount;
    allocatedAllocators[region]->hugeFree(POI);
}

pod *TopLevelAllocatorBundle::allocateNPages(size_t size) {
    TopLevelAllocator* instanceLocked = getTopLevelAllocator();

    pod* result = instanceLocked->allocateNPages(size);
    if(size > 1) {
        instanceLocked->huge++;
    } else {
        instanceLocked->small++;
    }
    
    instanceLocked->globalUnlock();

    return result;
}

TopLevelAllocator *TopLevelAllocatorBundle::addNewRegion() {
    muxLock.lock("add tla");
    loopAdd++;
    TopLevelAllocator* topLevelAllocatorInstance = addNewRegionCore();
    muxLock.unlock("add tla");
    return topLevelAllocatorInstance;
}

void TopLevelAllocatorBundle::reportStatistics() {
    stringstream ss;
    size_t index = 0;
    for(auto i : allocatedAllocators) {
        info(ss << "allocator " << index  << " huge: " << i->huge << " small: " << i->small);
        index++;
    }
}

TopLevelAllocator *TopLevelAllocatorBundle::perThreadGet() {
    muxLock.lock(std::string());
    auto searchIt = allocatorMap.find(this_thread::get_id());
    if (searchIt == allocatorMap.end()) {
        if (lastAllocated + localPodCount - 1 > pods.size()) {
            muxLock.unlock(std::string());
            return nullptr;
        }
        if(allocatedAllocators.size() == maxArena) {
            muxLock.unlock(std::string());
            return nullptr;
        }
        threadAdd++;
        auto *newSplBundle = addNewRegionCore();
        allocatorMap.insert(make_pair(this_thread::get_id(), newSplBundle));
        muxLock.unlock(std::string());
        return newSplBundle;
    } else {
        TopLevelAllocator* result = searchIt->second;
        result->globalLock();
        muxLock.unlock(std::string());
        return result;
    }
}

TopLevelAllocator *TopLevelAllocatorBundle::loopGet() {
    if(allocatedAllocators.size() == maxArena) {
        while(true) {
            for (auto allocatedTla: allocatedAllocators) {
                if (allocatedTla->globalTrylock()) {
                    return allocatedTla;
                }
            }
        }
    }
    if (allocatedAllocators.empty()) {
        return addNewRegion();
    } else {
        for (auto allocatedTla: allocatedAllocators) {
            if (allocatedTla->globalTrylock()) {
                return allocatedTla;
            }
        }
        return addNewRegion();
    }
}

void TopLevelAllocatorBundle::trim() {
    for(int i = 0; i < allocatedAllocators.size(); i--) {
        allocatedAllocators[i]->trim();
    }
}

TopLevelAllocator *TopLevelAllocatorBundle::addNewRegionCore() {
    auto* topLevelAllocatorInstance = new TopLevelAllocator(podsRef, localPodCount, base, pageSize, lastAllocated, lastAllocated + localPodCount - 1);
    allocatedAllocators.push_back(topLevelAllocatorInstance);
    lastAllocated += localPodCount;
    topLevelAllocatorInstance->globalLock();
    return topLevelAllocatorInstance;
}
