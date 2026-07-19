#include "remon.h"

// Return OS thread id (tid)
static inline unsigned long GetCurrentThreadID() {
    return (unsigned long) syscall(SYS_gettid);
}


// mutex remon_vmm::seg_lock;
remon_vmm *remon_vmm::remonSelfPtr;
// profilers
profiler hugeMallocProfiler("huge_malloc");

profiler reusePodProfiler("reuse_pod_profiler");
profiler activatePageProfiler("activate_page_profiler");
profiler bufferHit("buffer_hit");
profiler LRURead("LRU_read");
profiler evictPodProfiler("evict_pod_profiler");
profiler LRUUpdateProfiler("LRU_update_profiler");
profiler remonFreeProfiler("remon_free_profiler");
profiler segfaultProfiler("segfault_profiler");
profiler activatePodWrapProfiler("activate_pod_wrap");


remon_vmm::remon_vmm() : log("remon_vmm " + to_string(getpid())), generator(rd()){

    _timeBegin = Clock::now();

    runTime = new profiler("vmm_run_time");
    runTime->record();

    profiler initTime("vmm_init_time");
    initTime.record();

    stringstream ss;
    info(ss << "remon version: " << versionStr);

    initVm();
    initPods();
    info(ss << "page_size: " << remonConfig.pageSize);

    remon_vmm::remonSelfPtr = this;
    segfaultRegistration();

    TopLevelAllocator = new TopLevelAllocatorBundle(&pods, podCount, base, remonConfig.pageSize);
    splMuxObj = new SplMux(remonConfig.pageSize - QSIZE);

    size_t bufferNormalizedSize = BUFF_POD * M1 / remonConfig.pageSize;
    info(ss << "currently the allocation buffer is: " << BUFF_POD << " MB");

    podBoundedBufferBundleObj = new BoundedBufferBundle(
            bufferNormalizedSize, TopLevelAllocator);
    ramWatcher = new RamWatch(remonConfig.maxRamInGB);
    peakRamThread = new thread(&remon_vmm::peakRamMain, this);
    peakRamThread->detach();
#ifdef PRE_SWAP
    preswapThread = new thread(&remon_vmm::preswapThreadMain, this);
    while(!preswapThreadRunning);
#endif

    freeLoopCv = new condition_variable();
    freeLoopMtx = new mutex();

    prefetchCv = new condition_variable();
    prefetchMtx = new mutex();

    int coreCount = getNcpu();
    if(coreCount >= MAX_PREFETCH) {
        coreCount = MAX_PREFETCH;
    }
    for(int i = 0; i < coreCount; i++) {
        thread* newWorkerThread = new thread(&remon_vmm::prefetchWorkerThreadMain, this);
        newWorkerThread->detach();
    }

    freeLoopThreadRunning = 1;

    for (const auto &peer: remonConfig.peers) {
        auto ptr = new RemoteConnectionBundle(peer, remonConfig.pageSize);
        remoteConnectionsMutex.lock();
        remoteConnections.push_back(ptr);
        remoteConnectionsMutex.unlock();
    }
    pid = getpid();
    if (remonConfig.swapDirPath.empty()) {
        swapDir = "";
    } else {
        swapDir = remonConfig.swapDirPath + "/" + to_string(pid);
    }
    if (!swapDir.empty()) {
        try {
            // Ensure SWAP_DISK parent exists before creating per-process swap dir.
            if (!filesystem::exists(remonConfig.swapDirPath)) {
                filesystem::create_directories(remonConfig.swapDirPath);
            }
            if (filesystem::exists(swapDir)) {
                remonConfig.clearSwapDir(pid, swapDir);
            } else {
                filesystem::create_directories(swapDir);
                info(ss << "created_swap_dir: " << swapDir);
            }
        } catch (const std::filesystem::filesystem_error &e) {
            warn(ss << "swap_dir init failed (" << e.what() << "), running without disk swaps");
            swapDir.clear();
        }
    }
    if (swapDir.empty()) {
        info(ss << "Running without disk swaps");
    }
    while (!prefetchThreadRunning);
    info(ss << "remon_vmm() finished");
    initTime.writeRecord();
}

remon_vmm::~remon_vmm() {
    runTime->writeRecord();
    stringstream ss;
    info(ss << "~remon_vmm starts");

    freeLoopThreadRunning = 0;
    preswapThreadRunning = 0;
    prefetchThreadRunning = 0;
    prefetchTaskQueueCv.notify_all();
    freeLoopCv->notify_all();
    prefetchCv->notify_all();
    for (auto freeLoopKeeperThread: freeLoopKeeperThreads) {
        freeLoopKeeperThread->join();
    }
    delete freeLoopCv;
    delete freeLoopMtx;
    delete prefetchCv;
    delete prefetchMtx;
    delete TopLevelAllocator;
    delete podBoundedBufferBundleObj;
#ifdef PRE_SWAP
    preswapThread->join();
#endif
    info(ss << "Peak memory: " << peakRam);
    reportPodStatusCounts();
    for (auto pod: pods) {
        delete pod;
    }
    filesystem::remove_all(swapDir);
    munmap(base, vmSizeGlobal);

    delete splMuxObj;
    delete ramWatcher;
    delete runTime;

    reportPrefetcherStatistics();

    info(ss << "~remon_vmm finished");
    info(ss << "this lib is built: " << buildDateAndTime);
}

void remon_vmm::initVm() {
    stringstream ss;
    vmSize = remonConfig.maxVmInGB * GB;
    podCount = vmSize / remonConfig.pageSize;
    podCountGlobal = podCount * remonConfig.topLevelArena;
    vmSizeGlobal = vmSize * remonConfig.topLevelArena;
    info(ss << "vm_size: " << vmSizeGlobal);
    info(ss << "# of managed pods: " << podCountGlobal);
    base = (uint8_t *) mmap(nullptr, vmSizeGlobal, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_POPULATE, -1,
                            0);
    if (base == MAP_FAILED) {
        err(ss << "Failed to allocate vm, need to exit");
        exit(-1);
    }

#ifdef USE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS(base, vmSizeGlobal);
#endif

    info(ss << "vm base is: " << reinterpret_cast<void *>(base));
}

void remon_vmm::initPods() {
    stringstream ss;
    u_int8_t *currMemory = (u_int8_t *) base;

    info(ss << "creating " << podCountGlobal << " pods");
    pods.resize(podCountGlobal, nullptr);
    mapForPodI(0);
    info(ss << "total " << pods.size() << " pods created");
}


void *remon_vmm::remonMalloc(size_t size) {
    remonGlobalLock("remon_malloc");
    if (size > remonConfig.pageSize - QSIZE) {
        hugeMallocProfiler.record();
        void *ptr = hugeMalloc(size);
        hugeMallocProfiler.writeRecord();
        if (ptr == nullptr) {
            stringstream ss;
            err(ss << "cannot malloc over page size: " << size);
        }

        remonGlobalUnlock();
        return ptr;
    }

    size = computeAsize(size);
    reusePodProfiler.record();
    SPLBundle *splBundle = splMuxObj->splGet();
    SegregatedPageList* spl = splBundle->getSplBasedOnSize(size);
    void *result = spl->allocate(size);
    if (result) {
        pod *podDidTheWork = getResponsiblePod(result);
        podDidTheWork->lock("malloc 2");
        LRUUpdateProfiler.record();
        updateLRUList(podDidTheWork);
        LRUUpdateProfiler.writeRecord();
        podDidTheWork->unlock("malloc 2");
        reusePodProfiler.writeRecord();
        remonGlobalUnlock();
        return result;
    }
    reusePodProfiler.writeRecord();
    bufferHit.record();
    pod *newPod = podBoundedBufferBundleObj->getPodBoundedBuffer()->getBufferedPod();
    bufferHit.writeRecord();
    if (newPod) {
        newPod->heapInit();
        void *ptr = spl->addPodAndAllocate(newPod, size);
        if (ptr) {
            LRUUpdateProfiler.record();
            updateLRUList(getResponsiblePod(ptr));
            LRUUpdateProfiler.writeRecord();
            newPod->unlock("remon_malloc");
            snapShot();
            remonGlobalUnlock();
            return ptr;
        }
        stringstream ss;
        warn(ss << "it should never reach here, new page always allocates");
        newPod->unlock("remon_malloc");
    }

    stringstream ss;
    err(ss << "failed to malloc: " << size);
    remonGlobalUnlock();
    return nullptr;
}

void remon_vmm::remonFree(void *addr) {
    remonFreeProfiler.record();
    remonGlobalLock("remon_free");
    pod *podOfInterest = getResponsiblePod(addr);
    if (podOfInterest->getPayload() == addr) {
        TopLevelAllocator->crossOwnershipHugeFree(podOfInterest);
    } else {
        podOfInterest->lock("free");
        auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
        if (spl->free(podOfInterest, addr)) {
            remonGlobalUnlock();
            activatePod(podOfInterest);
            podOfInterest->unlock("for flushing");
        } else {
            podOfInterest->unlock("free");
        }

    }
    remonGlobalUnlock();
    remonFreeProfiler.writeRecord();
}

void *remon_vmm::hugeMalloc(size_t size) {
    size_t numPageNeeded = ceil(((double) size) / remonConfig.pageSize);

    pod *ptr = TopLevelAllocator->allocateNPages(numPageNeeded);
    if (ptr == nullptr) {
        stringstream ss;
        err(ss << "huge malloc failed");
        return nullptr;
    }
    for (int i = ptr->getI(); i < ptr->getI() + ptr->globalSize; i++) {
        mapForPodI(i);
        pods[i]->headPod = ptr;
        pods[i]->lock("remon_malloc");
        if (pods[i]->getState() != podState::podStateT::local) {
            activatePod(pods[i]);
            LRUUpdateProfiler.record();
            updateLRUList(pods[i]);
            LRUUpdateProfiler.writeRecord();
        }
        pods[i]->isFree = false;
        pods[i]->unlock("remon_malloc");
    }

    return ptr->getPayload();
}

void remon_vmm::reportPodStatusCounts() {
    int local = 0;
    int localPinned = 0;
    int init = 0;
    int disk = 0;
    int remote = 0;
    int diskCache = 0;
    int remoteCache = 0;
    int pageForSmallAllocations = 0;
    int pageForHugeAllocations = 0;
    int notUsedNullPage = 0;
    double totalFrag = 0;
    int index = 0;
    for (pod *i: pods) {
        if(i == nullptr) {
            notUsedNullPage++;
            continue;
        }
        switch (i->getState()) {
            case podState::podStateT::local:
                local++;
                if (i->isForSmallAllocations()) {
                    pageForSmallAllocations++;
                } else {
                    pageForHugeAllocations++;
                }
                break;
            case podState::podStateT::init:
                init++;
                break;
            case podState::podStateT::disk:
                disk++;
                break;
            case podState::podStateT::localPinned:
                localPinned++;
                break;
            case podState::remote:
                remote++;
                break;
            case podState::diskCache:
                diskCache++;
                break;
            case podState::remoteCache:
                remoteCache++;
                break;
        }
        index++;
    }
    stringstream ss;
    double RAM_GB = ramWatcher->getRamUsage();
    info(ss << "pod counts: local: " << local << " local_pinned: " << localPinned << " init: " << init << " disk: "
            << disk << " remote: " << remote << " disk_cache: " << diskCache << " remote_cache: " << remoteCache << " not_used_null_page: "
            << notUsedNullPage << " ram: " << RAM_GB);
    info(ss << "pod counts (locals only): huge: " << pageForHugeAllocations << " small: "
            << pageForSmallAllocations);
}

size_t remon_vmm::computeAsize(size_t size) {
    size_t asize;
    if (size <= DSIZE) {
        asize = DSIZE + OVERHEAD;
    } else {
        asize = DSIZE * ((size + OVERHEAD + DSIZE - 1) / DSIZE); 
    }
    return asize;
}

void remon_vmm::printBacktrace() {
    stringstream ss;
    const int maxTraceSize = 20;
    void *stackTrace[maxTraceSize];
    int stackSize = backtrace(stackTrace, maxTraceSize);
    char **symbols = backtrace_symbols(stackTrace, stackSize);
    if (symbols == nullptr) {
        err(ss << "cannot dump symbols");
    }
    for (int i = 0; i < stackSize; i++) {
        int status;
        char *demangledSymbol = abi::__cxa_demangle(symbols[i], nullptr, nullptr, &status);
        if (demangledSymbol != nullptr) {
            debug(ss << "D:" << demangledSymbol);
            free(demangledSymbol);
        } else {
            debug(ss << symbols[i]);
        }
    }
    free(symbols);
}

void remon_vmm::activateAddressSeg(void *address) {
    activateAddress(address);
}

void remon_vmm::activateAddress(void *address) {
    long offset = (u_int8_t *) address - (u_int8_t *) base;
    int podI = offset / remonConfig.pageSize;
    stringstream buf;
    buf << address;

    if (podI < 0 || podI >= pods.size()) {
        stringstream ss;
        err(ss << "activating out of range address: " << address << " pod_i: " << podI);
        printBacktrace();
        exit(-1);
    }

    pod *podOfInterest = pods[podI];
    if (podOfInterest->getState() == podState::podStateT::local) {
        return;
    }

#ifdef PREFETCH
    activatePodWrapProfiler.record();
    activatePodWrap(podOfInterest, 0);
    activatePodWrapProfiler.writeRecord();
#endif
    podOfInterest->lock("segfault handling");
    segfaultProfiler.record();
    activatePod(podOfInterest);
    segfaultProfiler.writeRecord();
    podOfInterest->unlock("segfault handling");
}

bool remon_vmm::activatePod(pod *podOfInterest) {
    if(podOfInterest->getState() != podState::podStateT::local) {
        freeLoopSignal();
    }
    switch (podOfInterest->getState()) {
        case podState::podStateT::local:
            break;
        case podState::podStateT::localPinned:
            break;
        case podState::podStateT::init:
            podOfInterest->initToLocal();
            break;
        case podState::podStateT::disk:
            podOfInterest->diskToLocalPinned();
            if (podOfInterest->isForSmallAllocations()) {
                stringstream ss;
                auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
                spl->splGlobalLock("disk_to_local " + to_string(
                        podOfInterest->getI()));
                spl->unbenchSwappedPage(podOfInterest);
                spl->splGlobalUnlock("disk_to_local");
            }

            podOfInterest->localPinnedToLocal();
            break;
        case podState::podStateT::diskCache:
            podOfInterest->diskCacheToLocalPinned();
            if (podOfInterest->isForSmallAllocations()) {
                stringstream ss;
                auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
                spl->splGlobalLock("disk_cache_to_local_pinned " + to_string(
                        podOfInterest->getI()));
                spl->unbenchSwappedPage(podOfInterest);
                spl->splGlobalUnlock("disk_cache_to_local_pinned");
            }
            podOfInterest->localPinnedToLocal();
            prefetchHit++;
#ifdef PREFETCH
            activatePodWrapProfiler.record();
            activatePodWrap(podOfInterest, 0);
            activatePodWrapProfiler.writeRecord();
#endif
            break;
        case podState::podStateT::remoteCache:
            podOfInterest->remoteCacheToLocalPinned();
            if (podOfInterest->isForSmallAllocations()) {
                stringstream ss;
                auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
                spl->splGlobalLock("remote_cache_to_local_pinned " + to_string(
                        podOfInterest->getI()));
                spl->unbenchSwappedPage(podOfInterest);
                spl->splGlobalUnlock("remote_cache_to_local_pinned");
            }
            podOfInterest->localPinnedToLocal();
            prefetchHit++;
#ifdef PREFETCH
            activatePodWrapProfiler.record();
            activatePodWrap(podOfInterest, 0);
            activatePodWrapProfiler.writeRecord();
#endif
            break;
        case podState::podStateT::remote:
            podOfInterest->remoteToLocalPinned();
            if (podOfInterest->isForSmallAllocations()) {
                auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
                spl->splGlobalLock("remote_to_local");
                spl->unbenchSwappedPage(podOfInterest);
                spl->splGlobalUnlock("remote_to_local");
            }
            podOfInterest->localPinnedToLocal();
            break;
        default:
            stringstream ss;
            warn(ss << "unexpected state in activate pod: " << podOfInterest->getI() << " state: "
                    << podOfInterest->getState());
    }
    LRUUpdateProfiler.record();
    updateLRUList(podOfInterest);
    LRUUpdateProfiler.writeRecord();
    return true;
}

pod *remon_vmm::getResponsiblePod(void *addr) {
    if (addr == nullptr) {
        stringstream ss;
        warn(ss << "get_responsible_pod return nullptr due to passed in nullptr");
        return nullptr;
    }
    long offset = pointerMinusPointer(addr, base);
    long nth = offset / remonConfig.pageSize;
    return pods[nth];
}

void remon_vmm::segfaultRegistration() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = remon_vmm::segfaultHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
}

void remon_vmm::segfaultHandler(int signal, siginfo_t *si, void *arg) {
    remonSelfPtr->activateAddressSeg(si->si_addr);
}

void remon_vmm::pickPodToEvict() {
    LRURead.record();
    
    pod *podToEvict = getFromLRUList();
    if (podToEvict == nullptr) {
        return;
    }
    LRURead.writeRecord();

    switch (podToEvict->getState()) {
        case podState::podStateT::local:
#ifdef PRE_SWAP
            if(podToEvict->preswap) {
                podToEvict->preswapToLocal();
            }
#endif
            if (podToEvict->isForSmallAllocations()) {
                auto *spl = static_cast<SegregatedPageList *>(podToEvict->splPushdown);
                spl->splGlobalLock("LRU_list " + to_string(podToEvict->getI()));
                bool reallyRemovedFromSpl = spl->benchSwappedPage(podToEvict);
                spl->splGlobalUnlock("LRU_list");
                podToEvict->flushFreeBuffer();
                if(podToEvict->isEmpty()) {
                    podToEvict->localToInit();
                    break;
                }
                if(reallyRemovedFromSpl) {
                    evictPod(podToEvict);
                } else {
                    activatePod(podToEvict);
                }
            } else {
                if(podToEvict->isFree) {
                    podToEvict->localToInit();
                    break;
                }
                evictPod(podToEvict);
            }
            break;
        case podState::podStateT::diskCache:
            podToEvict->diskCacheToDisk();
            prefetchEvictCount++;
            break;
        case podState::podStateT::remoteCache:
            podToEvict->remoteCacheToRemote();
            prefetchEvictCount++;
            break;
        case podState::podStateT::localPinned:
            break;
        case podState::podStateT::init:
            break;
        default:
            stringstream ss;
            warn(ss << "unknown state in pick_pod_to_evict: " << podToEvict->getI()
                    << " state: " << podToEvict->getState());
            break;
    }
    podToEvict->unlock("pick_pod_to_evict");
    snapShot();
}

void remon_vmm::evictPod(pod *podToEvict) {
    evictPodProfiler.record();
    if (podToEvict->getState() != podState::local) {
        stringstream ss;
        warn(ss << podToEvict->getI() << " pod is not local, it is " << podToEvict->getState());
        podToEvict->unlock("evict_pod");
        return;
    }

    if (!remoteSwap(podToEvict)) {
        swapToDisk(podToEvict);
    }
    evictPodProfiler.writeRecord();
}

void remon_vmm::swapToDisk(pod *podToEvict) {
    string path = swapDir + "/" + to_string(podToEvict->getI());
    if (swapDir.empty()) {
        stringstream ss;
        err(ss << "we need to swap but did not configure swap_path");
        exit(-1);
    }
    podToEvict->localToDisk(path);
}

bool remon_vmm::remoteSwap(pod *podOfInterest) {
    remoteConnectionsMutex.lock();
    auto localCopy = remoteConnections;
    remoteConnectionsMutex.unlock();
    shuffle(localCopy.begin(), localCopy.end(), generator);
    for (auto connection: localCopy) {
        if (connection->savePodToRemote(podOfInterest)) {
            return true;
        }
    }
    return false;
}

void remon_vmm::freeLoopSignal() {
    RamWatch *localWatcher = checkTidMappedFreeThread(remonConfig.maxRamInGB);
    if (swapModeOn) {
        freeLoopCv->notify_one();
        while (true) {
            if (!localWatcher->ifNeedToSwap()) {
                break;
            }
            if (getSwappingThreads() < freeLoopKeeperThreads.size()) {
                freeLoopCv->notify_one();
            }
        }
    }

}

RamWatch *remon_vmm::checkTidMappedFreeThread(double maxRam) {
    splFreeLoopMux.lock();
    auto searchIt = splFreeLoopMap.find(this_thread::get_id());
    if (searchIt == splFreeLoopMap.end()) {

        auto ramWatchThreadSpecific = new RamWatch(maxRam);
        splFreeLoopMap.insert(make_pair(this_thread::get_id(), ramWatchThreadSpecific));
        auto *workerThread = new thread(&remon_vmm::freeLoopThreadSpl, this,
                                         ramWatchThreadSpecific);
        freeLoopKeeperThreads.push_back(workerThread);
        splFreeLoopMux.unlock();
        return ramWatchThreadSpecific;
    }
    splFreeLoopMux.unlock();
    return searchIt->second;
}

void remon_vmm::freeLoopThreadSpl(RamWatch *ramWatchThreadSpecific) {
    stringstream ss;
    swappingThreadsAdd();
    while (freeLoopThreadRunning) {
        memoryFreeLoopSpl(ramWatchThreadSpecific);
        swappingThreadsReduce();
        if (freeLoopThreadRunning == 0) {
            break;
        }
        unique_lock<mutex> lock(*freeLoopMtx);
        freeLoopCv->wait(lock);
        lock.unlock();
        swappingThreadsAdd();
    }
}

void remon_vmm::memoryFreeLoopSpl(RamWatch *ramWatchThreadSpecific) {
    int count = 0;
    while (ramWatchThreadSpecific->ifNeedToSwap()) {
        pickPodToEvict();
        count++;
        if (freeLoopThreadRunning == 0) {
            break;
        }
    }
}

pod *remon_vmm::getFromLRUList() {
    LRULock.lock();
    if (LRUList.empty()) {
        stringstream ss;
        err(ss << "no page to swap");
        LRULock.unlock();
        sleep(1);
        reportPodStatusCounts();
        return nullptr;
    }
    auto result = LRUList.back();
    if (result->tryLock("get_from_LRU_list")) {
        if (!result->LRUIterIsValid) {
            stringstream ss;
            err(ss << "LRU in get to swap is not valid");
        }
        result->LRUIterIsValid = false;
        LRUList.pop_back();

        LRULock.unlock();
        return result;
    }
    LRULock.unlock();
    return nullptr;
}

int remon_vmm::updateLRUList(pod *POI) { 
    if (POI == nullptr) {
        stringstream ss;
        warn(ss << "update_LRU_list: why pass in a nullptr");
        return 0;
    }

    LRULock.lock();
    if (LRUList.front() == POI) {
        LRULock.unlock();
        return 0;
    }
    if (!POI->LRUIterIsValid) {
        auto iterator = LRUList.insert(LRUList.begin(), POI);
        POI->rememberTheIterToLRUList(iterator);
        POI->LRUIterIsValid = true;
    } else {
        LRUList.erase(POI->LRUIter);
        auto iterator = LRUList.insert(LRUList.begin(), POI);
        POI->rememberTheIterToLRUList(iterator);
    }
    LRULock.unlock();
    return 1;
}

void remon_vmm::setFreeOnFooter(pod *podOfInterest) {
    assert(podOfInterest->isFree);
    assert(podOfInterest->globalSize != 0);
    size_t footerI = podOfInterest->getI() + podOfInterest->globalSize - 1;
    mapForPodI(footerI);
    pod *currFooterPod = pods[footerI];
    currFooterPod->isFree = true;
    assert(podOfInterest->globalSize == currFooterPod->globalSize);
    assert(currFooterPod->isFree);
}

void remon_vmm::verifyLRUList() {
    stringstream ss;
    if (hasDuplicates(LRUList)) {
        err(ss << "LRU list has duplicates");
    }
}

bool remon_vmm::hasDuplicates(const std::list<pod *> &myList) {
    std::unordered_set<pod *> encounteredElements;
    for (auto element: myList) {
        if (encounteredElements.find(element) != encounteredElements.end()) {
            return true;
        }
        encounteredElements.insert(element);
    }
    return false;
}

void remon_vmm::makeSureAllPodsInThisHugeAllocatedBlockIsNotSwapped(pod *podOfInterest) {
    stringstream ss;
    for (int i = 0; i < podOfInterest->globalSize; i++) {
        pod *currPod = pods[podOfInterest->getI() + i];
        switch (currPod->getState()) {
            case podState::podStateT::local:
                if (currPod->tryLock("local_to_init")) {
                    if (currPod->getState() == podState::podStateT::local) {
                        currPod->localToInit();
                    }
                    currPod->unlock("local_to_init");
                } else {
                    warn(ss << "cannot lock for local to init in big free");
                }
                break;
            case podState::podStateT::localPinned:
                cout
                        << "Warn: local_pinned should not show up in make_sure_all_pods_in_this_huge_allocated_block_is_not_swapped"
                        << endl;
                break;
            case podState::podStateT::disk:
                if (currPod->tryLock("disk_to_init")) {
                    if (currPod->getState() == podState::podStateT::disk) {
                        currPod->diskToInit();
                    }
                    currPod->unlock("disk_to_init");
                } else {
                    warn(ss << "cannot lock for disk to init in big free");
                }
                break;
            case podState::podStateT::remote:
                if (currPod->tryLock("remote_to_init")) {
                    if (currPod->getState() == podState::podStateT::remote) {
                        currPod->remoteToInit();
                    }
                    currPod->unlock("remote_to_init");
                } else {
                    warn(ss << "cannot lock for remote to init in big free");
                }
                break;
            case podState::init:
                break;
        }
    }
}

void remon_vmm::snapShot() {
    return;

    stringstream ss;
    snapShotLock.lock();
    string path = "/mnt/ram/snap_shot/snap_shot_" + to_string(snapShotI);
    ofstream fh(path);
    if (!fh.is_open()) {
        err(ss << "snap_shot failed: " << snapShotI);
        exit(1);
    }
    for (pod *i: pods) {
        if (i->tryLock("snap")) {
            if (i->getState() == podState::podStateT::local) {
                if (i->isForSmallAllocations()) {
                    fh << "local," << i->usageRatio() << "\n";
                } else {
                    fh << "local_big\n";
                }
            } else if (i->getState() == podState::podStateT::init) {
                fh << "init\n";
            } else if (i->getState() == podState::podStateT::disk) {
                fh << "disk\n";
            } else if (i->getState() == podState::podStateT::localPinned) {
                fh << "local_pinned\n";
            } else if (i->getState() == podState::podStateT::remote) {
                fh << "remote\n";
            }
            i->unlock("snap");
        } else {
            fh << "locked\n";
        }
    }
    fh.flush();
    fh.close();
    snapShotI++;
    snapShotLock.unlock();
}

void remon_vmm::checkInitLRU() {
    return;
    if (!firstSwapHappened()) {
        LRULock.lock();
        firstSwapLock.lock();
        if (didFirstSwapHappen) {
            firstSwapLock.unlock();
            LRULock.unlock();
            return;
        }
        auto podsCopy = pods;
        auto compareByEpoch = [](const pod *a, const pod *b) {
            return a->epoch > b->epoch;
        };
        std::sort(podsCopy.begin(), podsCopy.end(), compareByEpoch);
        for (auto POI: podsCopy) {
            if (POI->epoch != 1) {
                auto iterator = LRUList.insert(LRUList.begin(), POI);
                POI->rememberTheIterToLRUList(iterator);
                POI->LRUIterIsValid = true;
            }
        }
        stringstream ss;
        info(ss << "initial insert LRU entry: " << LRUList.size());
        didFirstSwapHappen = true;
        firstSwapLock.unlock();
        LRULock.unlock();
    }
}

profiler trimHugeProfiler("trim_huge");
profiler trimSmallProfiler("trim_small");

void remon_vmm::peakRamMain() {
    double currRam = 0;
    RamWatch ramWatchThreadSpecific(remonConfig.maxRamInGB);
    while (true) {
        currRam = ramWatchThreadSpecific.getRamUsage();
        if (currRam > peakRam) {
            peakRam = currRam;
        }
        if(!swapModeOn) {
            if (currRam > remonConfig.maxRamInGB - (((double) remonConfig.pageSize * 32.0) / GB)) {
                freeLoopCv->notify_all();
                swapModeOn = true;
                TopLevelAllocator->lockLoopOn = true;
                trimHugeProfiler.record();
                TopLevelAllocator->trim();
                trimHugeProfiler.writeRecord();
                trimSmallProfiler.record();
                splMuxObj->trim();
                trimSmallProfiler.writeRecord();
                stringstream ss;
                info(ss << "swap on: curr ram: " << currRam);
            }
        } else {
            if (currRam >= remonConfig.maxRamInGB) {
                freeLoopCv->notify_one();
            }
        }
        this_thread::sleep_for(std::chrono::microseconds(10000));
    }
}

int remon_vmm::remonPromote(void *addr) {
    auto asyncThread = std::thread(&remon_vmm::remonPromoteAsync, this, addr);
    asyncThread.detach();
    return 0;
}

int remon_vmm::remonPromoteAsync(void *addr) {
    pod *podOfInterest = getResponsiblePod(addr);
    return remonPromotePod(podOfInterest);
}

void remon_vmm::asyncPromote(pod *podOfInterest) {
    podOfInterest->lock("async_promote");
    activatePod(podOfInterest);
    podOfInterest->unlock("async_promote");
}

int remon_vmm::remonDemote(void *addr) {
    pod *podOfInterest = getResponsiblePod(addr);
    podOfInterest->lock("demote");
    auto state = podOfInterest->getState();
    if (state == podState::podStateT::remote || state == podState::podStateT::disk) {
        podOfInterest->unlock("demote");
        return 0;
    }
    int ret = tailPodInLRUList(podOfInterest);
    podOfInterest->unlock("demote");
    return ret;
}

int remon_vmm::tailPodInLRUList(pod *POI) {
    if (POI == nullptr) {
        stringstream ss;
        warn(ss << "why pass in a nullptr in tail_pod_in_LRU_list");
        return 0;
    }
    LRULock.lock();
    if (LRUList.back() == POI) {
        LRULock.unlock();
        return 0;
    }
    if (!POI->LRUIterIsValid) {
        auto iterator = LRUList.insert(LRUList.end(), POI);
        POI->rememberTheIterToLRUList(iterator);
        POI->LRUIterIsValid = true;
    } else {
        LRUList.erase(POI->LRUIter);
        auto iterator = LRUList.insert(LRUList.end(), POI);
        POI->rememberTheIterToLRUList(iterator);
    }
    LRULock.unlock();
    return 1;
}

int remon_vmm::remonForceSwap(void *addr) {
    pod *podOfInterest = getResponsiblePod(addr);
    int ret = removePodFromLRU(podOfInterest);
    if (ret < 0) {
        return -1;
    }
    auto asyncThread = std::thread(&remon_vmm::asyncSwapOutPod, this, podOfInterest);
    asyncThread.detach();
    return 0;
}

int remon_vmm::removePodFromLRU(pod *podOfInterest) {
    LRULock.lock();
    if (LRUList.empty()) {
        stringstream ss;
        err(ss << "no page to swap");
        LRULock.unlock();
        return -1;
    }
    if (podOfInterest->tryLock("remove_pod_from_LRU")) {
        if (!podOfInterest->LRUIterIsValid) {
            stringstream ss;
            err(ss << "LRU in get to swap is not valid");
            podOfInterest->unlock("remove_pod_from_LRU");
            return -1;
        }
        LRUList.erase(podOfInterest->LRUIter);
        podOfInterest->LRUIterIsValid = false;
        LRULock.unlock();
        return 1;
    }
    LRULock.unlock();
    return 0;
}

void remon_vmm::asyncSwapOutPod(pod *podOfInterest) {
    stringstream ss;
    if (podOfInterest->getState() == podState::podStateT::local) { 
        if (podOfInterest->isForSmallAllocations()) {
            auto *spl = static_cast<SegregatedPageList *>(podOfInterest->splPushdown);
            spl->splGlobalLock("LRU_list " + to_string(podOfInterest->getI()));
            spl->benchSwappedPage(podOfInterest);
            spl->splGlobalUnlock("LRU_list");
            evictPod(podOfInterest);
        } else {
            evictPod(podOfInterest);
        }
    }
    podOfInterest->unlock("remon_force_swap");
}

int remon_vmm::remonPromoteRange(void *addrFrom, void *addrTo) {
    auto asyncThread = std::thread(&remon_vmm::remonPromoteRangeAsync, this, addrFrom, addrTo);
    asyncThread.detach();
    return 0;
}

void remon_vmm::remonPromoteRangeAsync(void *from, void *to) {
    if (from >= to) {
        stringstream ss;
        err(ss << "remon_promote_range_async: " << from << " ~ " << to << "does not enclose a range");
        return;
    }

    pod *fromPod = getResponsiblePod(from);
    pod *toPod = getResponsiblePod(to);

    for (int i = fromPod->getI(); i <= toPod->getI(); i++) {
        activatePod(pods[i]);
    }
}

int remon_vmm::remonPromotePod(pod *podOfInterest) {
    podOfInterest->lock("promote");
    auto state = podOfInterest->getState();
    if (state == podState::podStateT::remote || state == podState::podStateT::disk) {
        // better not sync here
        activatePod(podOfInterest);
        podOfInterest->unlock("promote");
        return 2;
    }
    int ret = updateLRUList(podOfInterest);
    podOfInterest->unlock("promote");
    return ret;
}

//===--------------------------------------------------------------------===//
// Synchronous APIs for DuckDB Buffer Manager Integration
//===--------------------------------------------------------------------===//

int remon_vmm::remonActivateSync(void *addr) {
    pod *podOfInterest = getResponsiblePod(addr);
    if (podOfInterest == nullptr) {
        stringstream ss;
        err(ss << "remonActivateSync: null pod for address " << addr);
        return -1;
    }

    podOfInterest->lock("activate_sync");
    auto state = podOfInterest->getState();

    // Already local - just update LRU and return
    if (state == podState::podStateT::local || state == podState::podStateT::localPinned) {
        updateLRUList(podOfInterest);
        podOfInterest->unlock("activate_sync");
        return 0;  // Already local, no fetch needed
    }

    // Need to fetch from remote/disk
    bool result = activatePod(podOfInterest);
    podOfInterest->unlock("activate_sync");
    return result ? 1 : -1;
}

void remon_vmm::remon_cleanup_evicted(void *addr) {
    pod *podOfInterest = getResponsiblePod(addr);
    if (podOfInterest == nullptr) {
        stringstream ss;
        warn(ss << "remon_cleanup_evicted: null pod for address " << addr);
        return;
    }

    podOfInterest->lock("cleanup_evicted");
    auto state = podOfInterest->getState();

    // Only need to clean up if in remote or disk state
    // toInit() handles all state transitions properly
    if (state != podState::podStateT::local && state != podState::podStateT::init) {
        podOfInterest->toInit();
    }
    podOfInterest->unlock("cleanup_evicted");
}

int remon_vmm::remonForceSwapRange(void *addrFrom, void *addrTo, void *exclude) {
    auto asyncThread = std::thread(&remon_vmm::remonForceSwapRangeAsync, this, addrFrom, addrTo, exclude);
    asyncThread.detach();
    return 0;
}


void remon_vmm::remonForceSwapRangeAsync(void *from, void *to, void *exclude) {
    if (from >= to) {
        stringstream ss;
        err(ss << "remon_force_swap_range_async: " << from << " ~ " << to << "does not enclose a range");
        return;
    }
    
    pod *fromPod = getResponsiblePod(from);
    pod *toPod = getResponsiblePod(to);
    pod *excludePod = getResponsiblePod(exclude);

    for (int i = fromPod->getI(); i <= toPod->getI(); i++) {
        if (pods[i] != excludePod) {
            remonForceSwap(pods[i]);
        }
    }
}

int remon_vmm::remonForceUnmap(void *addrFrom, void *addrTo, void *exclude) {
    auto asyncThread = std::thread(&remon_vmm::remonForceUnmapAsync, this, addrFrom, addrTo, exclude);
    asyncThread.detach();
    return 0;
}

void remon_vmm::remonForceUnmapAsync(void *from, void *to, void *exclude) {
    if (from >= to) {
        stringstream ss;
        err(ss << "remon_force_unmap_async: " << from << " ~ " << to << "does not enclose a range");
        return;
    }
    
    pod *fromPod = getResponsiblePod(from);
    pod *toPod = getResponsiblePod(to);
    pod *excludePod = getResponsiblePod(exclude);

    for (int i = fromPod->getI(); i <= toPod->getI(); i++) {
        if (pods[i] != excludePod) {
            pods[i]->toInit();
        }
    }
}

int remon_vmm::remonDemoteRange(void *addrFrom, void *addrTo, void *exclude) {
    auto asyncThread = std::thread(&remon_vmm::remonDemoteRangeAsync, this, addrFrom, addrTo, exclude);
    asyncThread.detach();
    return 0;
}

void remon_vmm::remonDemoteRangeAsync(void *from, void *to, void *exclude) {
    if (from >= to) {
        stringstream ss;
        warn(ss << "remon_demote_range_async: " << from << " ~ " << to << " does not enclose a range");
        return;
    }
    
    pod *fromPod = getResponsiblePod(from);
    pod *toPod = getResponsiblePod(to);
    pod *excludePod = getResponsiblePod(exclude);

    for (int i = fromPod->getI(); i <= toPod->getI(); i++) {
        if (pods[i] != excludePod) {
            remonDemote(pods[i]->getPayload());
        }
    }
}


void remon_vmm::markLastAFewPageAsPreswap() {
#ifdef PRE_SWAP
    vector<pod*> targets;
    LRULock.lock();
    std::list<pod*>::reverseIterator rit = LRUList.rbegin();
    int item = LRUList.size() / 20;
    if(item > 0) {
        while (rit != LRUList.rend()) {
            if (item == 0) {
                break;
            }
            targets.push_back(*rit);
            rit++;
            item--;
        }
    }
    LRULock.unlock();

    for(auto target : targets) {
        if(target->tryLock("preswap")) {
            if(target->getState() == podState::podStateT::local) {
                if(!target->preswap) {
                    target->localToPreswap();
                }
            }
            target->unlock("preswap");
        }
    }
#endif
}

void remon_vmm::preswapThreadMain() {
    preswapThreadRunning = 1;
    stringstream ss;
    while (preswapThreadRunning) {
        sleep(1);
        markLastAFewPageAsPreswap();
    }
}

void remon_vmm::prefetch(pod *POI, size_t knownPrefetchSize) {
    if(knownPrefetchSize) {
        for(int i = POI->getI(); i < POI->getI() + knownPrefetchSize; i++) {
            pod* taskPod =pods[i];
            if (taskPod->tryLock("prefetch")) {
                prefetchPod(taskPod);
                taskPod->unlock("prefetch");
            }
        }
        return;
    }
    stringstream ss;
    vector<pod *> taskCombo;
    taskCombo.reserve(MAX_PREFETCH);

    if (!POI->isForSmallAllocations()) {
        size_t currI = POI->getI() + PREFETCH_DELAY;
        pod *head = POI->headPod;

        int count = 1;
        while (currI < pods.size()) {
            if (pods[currI]->headPod != head) {
                break;
            }
            if (count == MAX_PREFETCH) {
                break;
            }
            
            taskCombo.push_back(pods[currI]);

            currI++;
            count++;
        }
    }
    
    for (auto taskPod: taskCombo) {
        if (taskPod->getState() == podState::podStateT::diskCache ||
            taskPod->getState() == podState::podStateT::remoteCache) {
            continue;
        }
        if (taskPod->tryLock("prefetch")) {
            prefetchPod(taskPod);
            taskPod->unlock("prefetch");
        }
    }
}

void remon_vmm::prefetchWorkerThreadMain() {
    prefetchThreadRunning = 1;
    while (true) {
        pod* target = nullptr;
        size_t knownPrefetchSize = 0;
        {
            std::unique_lock<std::mutex> lock(prefetchTaskQueueMtx);
            prefetchTaskQueueCv.wait(lock);
            if(!prefetchThreadRunning) {
                return;
            }
            if(prefetchTaskQueue.empty()) {
                continue;
            }

            if (!prefetchTaskQueue.empty()) {
                auto task = prefetchTaskQueue.front();
                prefetchTaskQueue.pop();
                target = task.first;
                knownPrefetchSize = task.second;
            }
        }

        if (target == nullptr) {
            continue;
        }
        
        prefetch(target, 0);
    }
}

void remon_vmm::activatePodWrap(pod *POI, size_t knownPrefetch) {
    {
        std::lock_guard<std::mutex> lock(prefetchTaskQueueMtx);
        prefetchTaskQueue.push(make_pair(POI, knownPrefetch));
    }

    prefetchTaskQueueCv.notify_one();
}

void remon_vmm::prefetchPod(pod *POI) {
    stringstream ss;
    switch (POI->getState()) {
        case podState::podStateT::disk:
            POI->diskToDiskCache();
            prefetchCount++;
            break;
        case podState::podStateT::remote:
            POI->remoteToRemoteCache();
            prefetchCount++;
            break;
        case podState::podStateT::init:
            POI->initToLocal();
            break;
        case podState::podStateT::local:
            return;
        case podState::podStateT::localPinned:
            return;
        case podState::podStateT::diskCache:
            return;
        case podState::podStateT::remoteCache:
            return;
        default:
            warn(ss << "prefetch_pod unexpected state: " << POI->getI() << " state: " << POI->getState());
            return;
    }
    LRUUpdateProfiler.record();
    updateLRUList(POI);
    LRUUpdateProfiler.writeRecord();
}

void remon_vmm::reportPrefetcherStatistics() {
    stringstream ss;
    info(ss << "prefetch_count: " << prefetchCount << " prefetch_hit: " << prefetchHit << " prefetch_evict_count: " << prefetchEvictCount);
    info(ss << "hit rate: " << prefetchHit * 1.0 / prefetchCount * 100 << " %");
}

int remon_vmm::getNcpu() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

void remon_vmm::mapForPodI(int i) {
    if(pods[i] == nullptr) {
        void *currMemory = (u_int8_t *) base + i * remonConfig.pageSize;
        pods[i] = new pod(remonConfig.pageSize, currMemory, i, -2);
    }
}

static void print_stack(void)
{
    void *buffer[1024];
    int nptrs = backtrace(buffer, 1024);

    backtrace_symbols_fd(buffer, nptrs, fileno(stdout));
}

static int meminfo_c = 0;

void* remon_vmm::remon_malloc(size_t size) {
    auto ptr = remonMalloc(size);
    //auto ptr = ::malloc(size);
    char buf[256];
    auto timeE = Clock::now() - _timeBegin;
    sprintf(buf, "MEM_INFO a %lu %p %lu %ld\n", size, ptr,
            GetCurrentThreadID(), timeE.count());
    stringstream ss;
    info(ss << buf);

    if ((size == 688128 || size == 1052672) && meminfo_c == 0) {
        print_stack();
    }
    return ptr;
}

void remon_vmm::remon_free(void* addr) {
    //::free(addr);return;
    remonFree(addr);
    char buf[256];
    auto timeE = Clock::now() - _timeBegin;
    sprintf(buf, "MEM_INFO f %p %lu %ld\n", addr,
            GetCurrentThreadID(), timeE.count());
    stringstream ss;
    //info(ss << buf);
}

