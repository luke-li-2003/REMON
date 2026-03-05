#include "TopLevelAllocator.h"

TopLevelAllocator::TopLevelAllocator(vector<pod *> *podsPtr, size_t podCount, void *basePtr, size_t pageSize,
                                         size_t iFrom,
                                         size_t iTo) :
                                            pods(*podsPtr),
                                            log("top_level_allocator"){
    this->podCount = podCount;
    base = basePtr;
    this->pageSize = pageSize;
    initGlobalFreePodList(iFrom, iTo);
}

void TopLevelAllocator::initGlobalFreePodList(size_t fromIndex, size_t toIndex) {
    from = fromIndex;
    to = toIndex;
    
    mapForPodI(from);
    mapForPodI(to);

    pod *firstPod = pods[from];
    firstPod->globalSize = podCount;
    firstPod->prev = nullptr;
    firstPod->next = nullptr;
    firstPod->isFree = true;

    pod *footerPod = pods[to];
    footerPod->isFree = true;
    footerPod->globalSize = podCount;
    
    globalLock();
    hugePodMapAddEntry(firstPod, std::string());
    globalUnlock();
}

void TopLevelAllocator::hugePodMapAddEntry(pod *podOfInterest, string reason) {
    size_t sizeClass = podOfInterest->globalSize;
    
    auto checkIt = hugePodMap.find(sizeClass);
    if (checkIt == hugePodMap.end()) {
        auto result = hugePodMap.insert(make_pair(sizeClass, podOfInterest));
        podOfInterest->prev = nullptr;
        podOfInterest->next = nullptr;
        setFreeOnFooter(podOfInterest);
        return;
    }
    
    pod *freeHeadPod = checkIt->second;
    podOfInterest->next = freeHeadPod;
    if (freeHeadPod) {
        freeHeadPod->prev = podOfInterest;
    }
    freeHeadPod = podOfInterest;
    podOfInterest->prev = nullptr;
    checkIt->second = freeHeadPod;
    dumpGlobalFreePodList("add_entry_2");
    setFreeOnFooter(podOfInterest);
    dumpGlobalFreePodList("add_entry_3");
}

void TopLevelAllocator::setFreeOnFooter(pod *podOfInterest) {
    assert(podOfInterest->isFree);
    assert(podOfInterest->globalSize != 0);
    size_t footerI = podOfInterest->getI() + podOfInterest->globalSize - 1;
    mapForPodI(footerI);
    pod *currFooterPod = pods[footerI];
    currFooterPod->isFree = true;
    currFooterPod->globalSize = podOfInterest->globalSize;
    assert(podOfInterest->globalSize == currFooterPod->globalSize);
    assert(currFooterPod->isFree);
}

pod *TopLevelAllocator::allocateNPages(int numPageNeeded) {
    if (numPageNeeded == 0) {
        return nullptr;
    }

    auto listOfCandidates = hugePodMap.lower_bound(numPageNeeded);
    pod *freeHeadPod = listOfCandidates->second;
    pod *currPod = freeHeadPod; 
    while (currPod) {
        if (currPod->globalSize >= numPageNeeded) {
            if (freeHeadPod == currPod) {
                freeHeadPod = currPod->next;
                if (currPod->next != nullptr) {
                    freeHeadPod->prev = nullptr;
                }
            }
            if (currPod->prev != nullptr) {
                currPod->prev->next = currPod->next;
            }
            if (currPod->next != nullptr) {
                currPod->next->prev = currPod->prev;
            }
            size_t residualSize = currPod->globalSize - numPageNeeded;
            currPod->globalSize = numPageNeeded;
            currPod->isFree = false;
            
            if (numPageNeeded > 1) {
                size_t footerI = currPod->getI() + numPageNeeded - 1;
                mapForPodI(footerI);
                pod *currFooterPod = pods[footerI];
                currFooterPod->globalSize = numPageNeeded;
                currFooterPod->isFree = false;
            }

            listOfCandidates->second = freeHeadPod;
            if (listOfCandidates->second == nullptr) {
                hugePodMap.erase(listOfCandidates);
            }
            if (residualSize > 0) {
                size_t residualI = currPod->getI() + numPageNeeded;
                mapForPodI(residualI);
                pod *residualPod = pods[residualI];
                residualPod->globalSize = residualSize;
                residualPod->isFree = true;
                
                if (residualSize > 1) {
                    size_t residualFooterI = residualPod->getI() + residualSize - 1;
                    mapForPodI(residualFooterI);
                    pod *residualFooterPod = pods[residualFooterI];
                    residualFooterPod->globalSize = residualSize;
                    residualFooterPod->isFree = true;
                }

                hugePodMapAddEntry(residualPod, "allocate_n_pages residual");
                dumpGlobalFreePodList("residual append");
            }

            dumpGlobalFreePodList("alloc 1");
            return currPod;
        }
        
        currPod = currPod->next;
    }
    dumpGlobalFreePodList("alloc failed");

    stringstream ss;
    debug(ss << "failed to allocate_n_pages");
    
    return nullptr;
}

void TopLevelAllocator::mapForPodI(size_t i) {
    if(pods[i] == nullptr) {
        void *currMemory = (u_int8_t *) base + i * pageSize;
        pods[i] = new pod(pageSize, currMemory, i, -2);
    }
}

void TopLevelAllocator::hugePodMapRemoveEntry(pod *podOfInterest) {
    dumpGlobalFreePodList(string("before_remove_entry: ") + to_string(podOfInterest->getI()));
    size_t sizeClass = podOfInterest->globalSize;
    
    auto checkIt = hugePodMap.find(sizeClass);
    if (checkIt == hugePodMap.end()) {
        stringstream ss;
        err(ss << "huge free remove entry error, cannot find size class list");
        return;
    }

    pod *freeHeadPod = checkIt->second;
    if (freeHeadPod != podOfInterest) {
        if (podOfInterest->prev != nullptr) {
            podOfInterest->prev->next = podOfInterest->next;
        }
        if (podOfInterest->next != nullptr) {
            podOfInterest->next->prev = podOfInterest->prev;
        }
    } else {
        if (podOfInterest->next == nullptr) {
            hugePodMap.erase(checkIt);
            assert(podOfInterest->prev == nullptr);
        } else {
            checkIt->second = podOfInterest->next;
            podOfInterest->next->prev = nullptr;
            podOfInterest->next = nullptr;
        }
    }
    dumpGlobalFreePodList("remove_entry");
}

pod *TopLevelAllocator::coalescing(pod *podOfInterest) {
    coalescingOnePodRight(podOfInterest);
    return podOfInterest;
}

void TopLevelAllocator::coalescingOnePodRight(pod *podOfInterest) {

    size_t currI = podOfInterest->getI();
    size_t freedSize = 0;
    if (podOfInterest->headPod != (pod *) 0xDEADBEEF && podOfInterest->headPod != podOfInterest) {
        stringstream ss;
        err(ss << "coalescing_one_pod_right: huge free error: middle of group huge malloc: " << podOfInterest->getI()
               << " head: " << podOfInterest->headPod);
    }

    while (true) {
        size_t nextHeaderI = currI + pods[currI]->globalSize;
        if(nextHeaderI > to) {
            break;
        }
        if(pods[currI]->globalSize == 0) {
            stringstream ss;
            err(ss << "curr_i: " << currI << " size: 0");
        }
        if (nextHeaderI >= pods.size()) {
            break;
        }
        pod *nextHeader = pods[nextHeaderI];
        if(nextHeader == nullptr) {
            stringstream ss;
            warn(ss << "next_header: " << nextHeaderI << " legal: " << from << " ~ " << to);
            break;
        }
        if (nextHeader->isFree) {
            pod *currPod = nextHeader;
            freedSize += currPod->globalSize;
            hugePodMapRemoveEntry(currPod);
            currI = nextHeaderI;
        } else {
            break;
        }
    }
    if (freedSize == 0) {
        return;
    }
    size_t newSize = freedSize + podOfInterest->globalSize;
    podOfInterest->globalSize = newSize;
    podOfInterest->isFree = true;
    
    if (newSize > 1) {
        pod *currFooterPod = pods[podOfInterest->getI() + newSize - 1];
        currFooterPod->globalSize = newSize;
        currFooterPod->isFree = true;
    }
}

pod *TopLevelAllocator::coalescingOnePodLeft(pod *podOfInterest) {
    size_t currI = podOfInterest->getI();
    size_t freedSize = 0;
    
    while (true) {
        if (currI - 1 < 0) {
            break;
        }
        pod *prevFooter = pods[currI - 1];
        pod *currPod = nullptr;
        if (prevFooter->isFree) {
            currPod = pods[currI];
            freedSize += currPod->globalSize;
            if (currPod == podOfInterest) {
                if (currPod->prev != nullptr) {
                    currPod->prev->next = currPod->next;
                }
                if (currPod->next != nullptr) {
                    currPod->next->prev = currPod->prev;
                }
                pod *currFooterPod = pods[currPod->getI() + currPod->globalSize - 1];
                currPod->globalSize = 0;
                currPod->isFree = false;
                currFooterPod->globalSize = 0;
                currFooterPod->isFree = false;
            } else {
                hugePodMapRemoveEntry(currPod);
            }
            currI -= (prevFooter->globalSize);
        } else {
            break;
        }
    }
    if (freedSize == 0) {
        return podOfInterest;
    }
    
    pod *enlargedPod = pods[currI];
    size_t newSize = freedSize + enlargedPod->globalSize;
    hugePodMapRemoveEntry(enlargedPod);
    enlargedPod->globalSize = newSize;
    enlargedPod->isFree = true;
    
    if (newSize > 1) {
        pod *currFooterPod = pods[enlargedPod->getI() + newSize - 1];
        currFooterPod->globalSize = newSize;
        currFooterPod->isFree = true;
    }
    return enlargedPod;
}

profiler hugeFreeProfiler("huge_free");
profiler hugeCoalescingProfiler("huge_coalescing");
void TopLevelAllocator::hugeFree(pod *podOfInterest) {
    hugeFreeProfiler.record();
    globalLock();
    dumpGlobalFreePodList("before_huge_free " + to_string(podOfInterest->getI()));
    // error check
    if (podOfInterest->headPod != nullptr && podOfInterest->headPod != podOfInterest) {
        stringstream ss;
        err(ss << "huge_free: huge free error: middle of group huge malloc: " << podOfInterest->getI() << " head: "
               << podOfInterest->headPod);
    }

    size_t oldSize = podOfInterest->globalSize;
    
    hugeCoalescingProfiler.record();
    podOfInterest = coalescing(podOfInterest);
    hugeCoalescingProfiler.writeRecord();

    stringstream ss;
    for (int i = podOfInterest->getI(); i < podOfInterest->getI() + oldSize; i++) {
        pods[i]->lock("huge_free");
        pods[i]->headPod = (pod *) 0xDEADBEEF;
        pods[i]->isFree = true;
        pods[i]->unlock("huge_free");
    }
    size_t numPages = podOfInterest->globalSize;
    podOfInterest->isFree = true;

    hugePodMapAddEntry(podOfInterest, "huge_free");
    hugeFreeProfiler.writeRecord();
    dumpGlobalFreePodList("after_huge_free");
    globalUnlock();
}

void TopLevelAllocator::trim() {
    globalLock();
    size_t podI = from;
    while (podI < to) {
        pod *header = pods[podI];
        size_t size = header->globalSize;
        size_t footerI = podI + size - 1;
        if(podI < from) {
            cout << "Err: from region failed bound check" << " from_i: " << podI << " legal region: " << from << " ~ " << to << endl;
            exit(-1);
        }
        if(footerI > to) {
            cout << "Err: from region failed bound check" << " footer_i: " << footerI << " legal region: " << from << " ~ " << to << endl;
            exit(-1);
        }
        pod *footer = pods[footerI];
        if (header->isFree && footer->isFree) {
            trimFreeBlock(podI, podI + size);
        } else if (!header->isFree && !footer->isFree) {
            // allocated page
        } else {
            stringstream ss;
            warn(ss << "Err: mismatch header and footer: " << header->isFree << " vs " << footer->isFree);
        }
        if (header->globalSize != footer->globalSize) {
            stringstream ss;
            warn(ss << "Err: mismatch header and size: " << header->globalSize << " vs " << footer->globalSize
                    << " head_i: " << header->getI() << " footer_i: " << footer->getI());
        }

        podI += size;
    }

    globalUnlock();
}

void TopLevelAllocator::trimFreeBlock(size_t startI, size_t endI) {
    for(size_t i = startI; i < endI; i++) {
        if(pods[i] == nullptr) {
            continue;
        }
        if(pods[i]->tryLock("trim")) {
            if(pods[i]->getState() == podState::podStateT::local) {
                pods[i]->dropMemoryStayLocal(true);
            }
            pods[i]->unlock("trim");
        }
    }
}


void TopLevelAllocator::dumpGlobalFreePodList(string reason) {
    if (PERF) {
        return;
    }

    stringstream ss;
    cout << "*******" << reason << "***************" << endl;

    for (auto it: hugePodMap) {
        pod *freeHeadPod = it.second;
        cout << "free pod block list class:" << it.first << endl;

        pod *curr = freeHeadPod;
        vector<bool> freeMap(podCount, false);

        cout << "   free pod block list:" << endl;
        cout << "       free_head_pod: " << freeHeadPod->getI() << " size: " << curr->globalSize << endl;
        while (curr) {
            cout << "       - free pod block: " << curr->getI() << " size: " << curr->globalSize << endl;
            size_t startI = curr->getI();
            size_t endI = curr->getI() + curr->globalSize - 1;
            for (size_t i = startI; i <= endI; i++) {
                if (!freeMap.at(i - from)) {
                    freeMap.at(i - from) = true;
                } else {
                    err(ss << "free region overlap! i = " << i << " start_i: " << startI << " ~ " << endI << " legal: " << from << " ~ " << to);
                }
            }
            
            if (curr->globalSize > 1) {
                pod *footerPod = pods[curr->getI() + curr->globalSize - 1];
                if (!footerPod->isFree) {
                    err(ss << "footer states not free: " << curr->getI());
                }
                if (footerPod->globalSize != curr->globalSize) {
                    err(ss << "footer states wrong size: " << curr->getI() << " should be " << curr->globalSize
                           << " but we have " << footerPod->globalSize);
                }
            }

            pod *next = curr->next;
            if (next) {
                if (next->prev) {
                    if (next->prev != curr) {
                        err(ss << "next prev is not curr, backward link error: " << curr->getI() << " points to: " <<
                               next->prev->getI());
                    }
                } else {
                    err(ss << "next prev is not curr, backward link to null: " << curr->getI());
                }
            }
            curr = next;
        }
        cout << "   End of free pod block list" << endl;
    }
    cout << "********************" << endl;

    // global_sanity_check
    size_t podI = from;
    while (podI < to) {
        pod *header = pods[podI];
        size_t size = header->globalSize;
        size_t footerI = podI + size - 1;
        if(podI < from) {
            cout << "Err: from region failed bound check" << " from_i: " << podI << " legal region: " << from << " ~ " << to << endl;
            exit(-1);
        }
        if(footerI > to) {
            cout << "Err: from region failed bound check" << " footer_i: " << footerI << " legal region: " << from << " ~ " << to << endl;
            exit(-1);
        }
        pod *footer = pods[footerI];
        if (header->isFree && footer->isFree) {
            debug(ss << "verifying: free huge_map: " << podI << " size: " << size << " reason: " << reason);
            auto iter = hugePodMap.find(header->globalSize);
            if (iter == hugePodMap.end()) {
                err(ss << "it does not even have the entry");
            } else {
                pod *curr = iter->second;
                pod *prev = curr;
                bool foundIt = false;
                while (true) {
                    debug(ss << "find entry in list: " << curr->getI() << " size: "
                           << curr->globalSize << " reason: " << reason);
                    if (curr == header) {
                        foundIt = true;
                        break;
                    }
                    if(curr->globalSize != prev->globalSize) {
                        err(ss << "list size mismatch: " << curr->globalSize << " vs " << prev->globalSize);
                        break;
                    }
                    if (curr->getI() + curr->globalSize == pods.size()) {
                        break;
                    }
                    prev = curr;
                    curr = curr->next;
                    if(!curr) {
                        break;
                    }
                }
                if (!foundIt) {
                    err(ss << "cannot find entry in huge_pod_map: " << prev->getI() << " size: "
                         << prev->globalSize << " reason: " << reason);
                }
            }
            debug(ss << "verified: free huge_map: " << podI << " reason: " << reason);
        } else if (!header->isFree && !footer->isFree) {
            // allocated page
        } else {
            cout << "Err: mismatch header and footer: " << header->isFree << " vs " << footer->isFree << endl;
        }
        if (header->globalSize != footer->globalSize) {
            warn(ss << "Err: mismatch header and size: " << header->globalSize << " vs " << footer->globalSize
                    << " head_i: " << header->getI() << " footer_i: " << footer->getI());
        }

        podI += size;
    }
    cout << "********************" << endl;
    cout << "********************" << endl;
}
