#include "SegregatedPageList.h"

profiler podTryMallocProfiler("pod_try_malloc");
profiler podFreeProfiler("pod_free");
profiler removeSplListProfiler("remove_spl_list_profiler");
profiler clearLocalFreeBufferProfiler("clear_local_free_buffer");

void *SegregatedPageList::allocate(size_t size) {
    splGlobalLock("allocate 0");

    void* resultFromLastUsed = tryAllocateWithPreviousPod(size);
    if(resultFromLastUsed) {
        splGlobalUnlock("allocate 0");
        return resultFromLastUsed;
    }
    if(lastAllocatedPod) {
        adjustPodList(lastAllocatedPod);
    }

    lastAllocatedPod = nullptr;

    size_t rankNeeded = size;
    auto mapIt = rankMap.begin();
    clearLocalFreeBufferProfiler.record();
    if(mapIt->first < rankNeeded) {
        if(localFreeBuffer.empty()) {
            splGlobalUnlock("allocate 1.1");
            return nullptr;
        }
        clearLocalFreeBuffer();
    }
    clearLocalFreeBufferProfiler.writeRecord();
    mapIt = rankMap.begin();
    if(mapIt->first < rankNeeded) {
        splGlobalUnlock("allocate 1");
        return nullptr;
    } else {
        void* result = allocateOfAList(size,  mapIt);
        splGlobalUnlock("allocate 2");
        return result;
    }

    return nullptr;
}

void SegregatedPageList::addPodFromSwappedState(pod* newPod) {
    stringstream ss;
    newPod->updateRank();
    size_t rank = newPod->rank;

    auto checkIt = rankMap.find(rank);
    if(checkIt == rankMap.end()) {
        auto result = rankMap.insert(make_pair(rank, new unordered_set<pod*>));
        checkIt = result.first;
    }
    checkIt->second->insert(newPod);
}

void* SegregatedPageList::addPodAndAllocate(pod* newPod, size_t size) {
    splGlobalLock("add_pod_and_allocate");
    newPod->splPushdown = this;
    newPod->heapInit();
    void* allocatedPtr = newPod->podMalloc(size);
    newPod->updateRank();
    auto checkIt = rankMap.find(newPod->rank);
    if(checkIt == rankMap.end()) {
        auto result = rankMap.insert(make_pair(newPod->rank, new unordered_set<pod*>));
        checkIt = result.first;
    }
    checkIt->second->insert(newPod);
    lastAllocatedPod = newPod;

    splGlobalUnlock("add_pod_and_allocate");
    return allocatedPtr;
}

bool SegregatedPageList::removePod(pod *podToRemove) {
    if(podToRemove == lastAllocatedPod) {
        return false;
    }
    podToRemove->updateRank();
    auto checkEntry = rankMap.find(podToRemove->rank);
    if(checkEntry == rankMap.end()) {
        stringstream ss;
        err(ss << "cannot find entry to remove in rank_map for remove_pod: " << podToRemove->getI() << " rank: " << podToRemove->rank);
        exit(-1);
    } else {
        removeSplListProfiler.record();
        checkEntry->second->erase(podToRemove);
        if(checkEntry->second->empty()) {
            delete(checkEntry->second);
            rankMap.erase(checkEntry);
        }
        removeSplListProfiler.writeRecord();
    }
    return true;
}

void SegregatedPageList::dumpDebug() {
    stringstream ss;
    splGlobalLock("dump_debug");
    debug(ss << "pods_used_for_small_allocations:");
    if(!rankMap.empty()) {
        debug(ss << "dump_debug: range: " << rankMap.begin()->first << " ~ " << rankMap.rbegin()->first);
    }
    for(auto & it : rankMap) {
        debug(ss << "rank: " << it.first << " count: " << it.second->size());
        for(auto & jt : *it.second) {
            if(it.first == 851952) {
                if (jt->isEmpty()) {
                    debug(ss << "page is empty: rank: " << it.first << " count: " << it.second->size());
                }
            }
        }
    }
    debug(ss << "end of pods_used_for_small_allocations");
    splGlobalUnlock("dump_debug");
}

SegregatedPageList::SegregatedPageList() : log("segregated_page_list"), lock("segregated_page_list"){
}



void *SegregatedPageList::allocateOfAList(size_t size, std::map<size_t, unordered_set<pod *> *>::iterator mapIt) {
    size_t oldRank = mapIt->first;
    unordered_set<pod *>* listOfPods = mapIt->second;
    for(auto it = listOfPods->begin(); it != listOfPods->end();it++) {
        podTryMallocProfiler.record();

        pod *podOfInterest = (*it);
        if (podOfInterest->tryLock("allocate_of_a_list")) {
#ifdef PRE_SWAP
            if(podOfInterest->preswap) {
                podOfInterest->preswapToLocal();
            }
#endif
            void *ptr = podOfInterest->podMalloc(size);
            podTryMallocProfiler.writeRecord();
            if (ptr) {
                lastAllocatedPod = podOfInterest;
                bool updated = podOfInterest->updateRank();

                if(updated) {
                    auto newRankIt = rankMap.find(podOfInterest->rank);
                    if (newRankIt == rankMap.end()) {
                        auto result = rankMap.insert(make_pair(podOfInterest->rank, new unordered_set<pod *>));
                        newRankIt = result.first;
                    }
                    auto result = newRankIt->second->insert(podOfInterest);

                    listOfPods->erase(it);
                    if (listOfPods->empty()) {
                        delete mapIt->second;
                        rankMap.erase(mapIt);
                    }
                }

                podOfInterest->unlock("allocate_of_a_list");
                return ptr;
            }
            podOfInterest->unlock("allocate_of_a_list");
        }
    }
    return nullptr;
}


SegregatedPageList::~SegregatedPageList() {
    for(const auto& pair : rankMap) {
        delete pair.second;
    }
}

bool SegregatedPageList::free(pod *podPtr, void *addr) {

    splGlobalLock("pod free: " + to_string(podPtr->getI()));

    auto state = podPtr->getState();
    if(state == podState::podStateT::disk || state == podState::podStateT::remote) {
        bool result = podPtr->addToFreeBufferReturnIfFull(addr);
        splGlobalUnlock("pod free A");
        return result;
    }
    if(!podPtr->addToFreeBufferReturnIfFull(addr)) {
        localFreeBuffer.insert(podPtr);
        splGlobalUnlock("pod free B");
        return false;
    }
    flushOnePage(podPtr);
    splGlobalUnlock("pod free C");
    return false;
}

bool SegregatedPageList::benchSwappedPage(pod* podToBench) {
    return removePod(podToBench);
}

void SegregatedPageList::unbenchSwappedPage(pod *podToUnbench) {
    addPodFromSwappedState(podToUnbench);
}

void SegregatedPageList::clearLocalFreeBuffer() {
    stringstream ss;
    vector<pod*> flushed;
    for(auto podPtr: localFreeBuffer) {
        if(podPtr->tryLock("flush")) {
            if(podPtr->getState() == podState::podStateT::local && podPtr->getFreeBufferSize() > FREE_FLUSH_SKIP) {
#ifdef PRE_SWAP
                if(podPtr->preswap) {
                    podPtr->preswapToLocal();
                }
#endif
                flushOnePage(podPtr);
                flushed.push_back(podPtr);
                podPtr->unlock("flush");
            } else {
                flushed.push_back(podPtr); 
                podPtr->unlock("flush");
            }
        }
    }
    for(auto podPtr : flushed) {
        localFreeBuffer.erase(podPtr);
    }
    // for(auto _pod: localFreeBuffer) {
    //     flushOnePage(_pod);
    // }
    // localFreeBuffer.clear();
}

profiler dropMemoryProfiler("drop_memory_profiler");

void SegregatedPageList::flushOnePage(pod *podPtr) {
    size_t oldRank = podPtr->rank;
    podPtr->flushFreeBuffer();
    bool updated = podPtr->updateRank();

    if(updated) {
        auto rankMapIt = rankMap.find(oldRank);
        if (rankMapIt != rankMap.end()) {
            removeSplListProfiler.record();
            rankMapIt->second->erase(podPtr);

            if (rankMapIt->second->empty()) {
                delete rankMapIt->second;
                rankMap.erase(rankMapIt);
            }
            removeSplListProfiler.writeRecord();
        } else {
            stringstream ss;
            err(ss << "flush_one_page should be fatal from now: " << "rank map cannot find the entry for the old rank, i = "
                   << podPtr->getI() << " old_rank: " << oldRank);
            exit(-1);

        }

        if (podPtr->getState() == podState::podStateT::local) {
            auto checkIt = rankMap.find(podPtr->rank);
            if (checkIt == rankMap.end()) {
                auto result = rankMap.insert(make_pair(podPtr->rank, new unordered_set<pod *>));
                checkIt = result.first;
            }
            checkIt->second->insert(podPtr);
        } else {
            stringstream ss;
            warn(ss << "freed pod is not local, no need to insert");
        }
    }
}

void *SegregatedPageList::tryAllocateWithPreviousPod(size_t size) {
    if(lastAllocatedPod == nullptr) {
        return nullptr;
    }
    size_t count = 0;
    while(true) {
        if (lastAllocatedPod->tryLock("try_allocate_with_previous_pod")) {
            if (lastAllocatedPod->getState() != podState::podStateT::local) {
                if(lastAllocatedPod->getState() == podState::podStateT::init) {
                    lastAllocatedPod->initToLocal();
                } else {
                    lastAllocatedPod->unlock("try_allocate_with_previous_pod");
                    stringstream ss;
                    warn(ss << "last_allocated becomes not local, state: " << lastAllocatedPod->getState() << " i: "
                            << lastAllocatedPod->getI());
                    return nullptr;
                }
            }
            void *result = lastAllocatedPod->podMalloc(size);
            lastAllocatedPod->unlock("try_allocate_with_previous_pod");
            if(count > 100) {
                return nullptr;
            }
            return result;
        }
        
        splGlobalUnlock("tmp unlock");
        splGlobalLock("regain lock in try same pod");
        count++;
    }
}

void SegregatedPageList::adjustPodList(pod *POI) {
    size_t oldRank = POI->rank;
    bool updated = POI->updateRank();
    size_t newRank = POI->rank;
    if(updated) {
        auto rankMapIt = rankMap.find(oldRank);
        if (rankMapIt != rankMap.end()) {
            removeSplListProfiler.record();
            rankMapIt->second->erase(POI);
            
            if (rankMapIt->second->empty()) {
                delete rankMapIt->second;
                rankMap.erase(rankMapIt);
            }
            removeSplListProfiler.writeRecord();
        } else {
            stringstream ss;
            err(ss << "adjust_pod_list should be fatal from now: " << "rank map cannot find the entry for the old rank, i = "
                   << POI->getI() << " old_rank: " << oldRank << " new_rank: " << newRank);
        }

        if (POI->getState() == podState::podStateT::local) {
            auto checkIt = rankMap.find(newRank);
            if (checkIt == rankMap.end()) {
                auto result = rankMap.insert(make_pair(newRank, new unordered_set<pod *>));
                checkIt = result.first;
            }
            checkIt->second->insert(POI);
        } else {
            stringstream ss;
            warn(ss << "freed pod is not local, no need to insert");
        }
    }
}

void SegregatedPageList::trim() {
    splGlobalLock("trim");
    vector<pod*> toRemove;
    for(auto & it : rankMap) {
        for(auto & jt : *it.second) {
            if(jt->tryLock("trim")) {
                if(jt->getState() == podState::podStateT::local) {
                    if (jt->isEmpty()) {
                        toRemove.push_back(jt);
                    }
                }
                jt->unlock("trim");
            }
        }
    }

    for(auto entry : toRemove) {
        if(entry->tryLock("trim")) {
            if(entry->getState() == podState::podStateT::local) {
                if (entry->isEmpty()) {
                    entry->dropMemoryStayLocal(false);
                }
            }
            entry->unlock("trim");
        }
    }
    splGlobalUnlock("trim");
}
