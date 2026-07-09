#ifndef REMON_H
#define REMON_H

#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <vector>
#include <thread>
#include <csignal>
#include <cmath>
#include <list>
#include <map>
#include <backtrace.h>
#include <semaphore.h>
#include "random"
#include <algorithm>

#include <chrono>

#include "unistd.h"
#include <execinfo.h>
#include <cxxabi.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <queue>

#include <valgrind/memcheck.h>

#include "shared/log.h"
#include "shared/profiler.h"
#include "shared/pod.h"
#include "SplMux.h"
#include "shared/define.h"
#include "RamWatch.h"
#include "config.h"
#include "RemoteConnectionBundle.h"
#include "TopLevelAllocator.h"
#include "TopLevelAllocatorBundle.h"
#include "BoundedBufferBundle.h"
#include "SpinLock.h"

using Clock = std::chrono::steady_clock;
using namespace std;

class remon_vmm : public log {
private:
    config remonConfig;
    string swapDir = "./swaps";
    string buildDateAndTime = __DATE__ " " __TIME__;
    RamWatch* ramWatcher;
    string versionStr = "1.16";
    pid_t pid;
    
    size_t podCount = 0;
    size_t podCountGlobal = 0;
    vector<pod*> pods;
    void* base;
    void initVm();
    void initPods();
    void segfaultRegistration();
    size_t vmSize;
    size_t vmSizeGlobal;

    profiler* runTime;
    SplMux* splMuxObj;

    Clock::time_point _timeBegin;

    void activateAddress(void* address);

    volatile bool didFirstSwapHappen = false;
    mutex firstSwapLock;
    bool firstSwapHappened() {
        firstSwapLock.lock();
        bool snapshot = didFirstSwapHappen;
        firstSwapLock.unlock();
        return snapshot;
    }

    mutex LRULock;
    list<pod*> LRUList;
    pod *getFromLRUList();
    int updateLRUList(pod *POI);

    BoundedBufferBundle* podBoundedBufferBundleObj;

    thread* peakRamThread = nullptr;
    void peakRamMain();
    double peakRam = 0;

    volatile int preswapThreadRunning = 0;
    thread* preswapThread = nullptr;
    void preswapThreadMain();

    volatile int prefetchThreadRunning = 0;
    thread* prefetchThread = nullptr;
    mutex* prefetchMtx;
    condition_variable* prefetchCv;
    mutex prefetchTargetMutex;
    pod* prefetchTarget = nullptr;
    queue<pair<pod*, size_t>> prefetchTaskQueue;
    mutex prefetchTaskQueueMtx;
    condition_variable prefetchTaskQueueCv;
    vector<thread*> prefetchWorkerThreads;
    void prefetchWorkerThreadMain();

    std::atomic<size_t> prefetchCount {0};
    std::atomic<size_t> prefetchEvictCount {0};
    std::atomic<size_t> prefetchHit {0};

    vector<thread*> freeLoopKeeperThreads;
    volatile int freeLoopThreadRunning = 0;
    mutex* freeLoopMtx;
    condition_variable* freeLoopCv;

    unordered_map<thread::id, RamWatch*> splFreeLoopMap;
    mutex splFreeLoopMux;
    void freeLoopThreadSpl(RamWatch *ramWatchThreadSpecific);
    atomic_int swappingThreads {0};

    void swappingThreadsAdd(){
        swappingThreads++;
    }

    void swappingThreadsReduce(){
        swappingThreads--;
    }

    int getSwappingThreads(){
        int snap = swappingThreads;
        return snap;
    }

    atomic_bool swapModeOn {false};

    void *hugeMalloc(size_t size);

    TopLevelAllocatorBundle * TopLevelAllocator;

    void reportPodStatusCounts();
    size_t computeAsize(size_t size);
    void printBacktrace();

    pod *getResponsiblePod(void *addr);
    void pickPodToEvict();
    void evictPod(pod *podToEvict);
    void swapToDisk(pod *pPod);

    std::mutex remoteConnectionsMutex;
    vector<RemoteConnectionBundle*> remoteConnections;
    std::random_device rd;
    std::mt19937 generator;

    string lockReason;
    void remonGlobalLock(string reason) {};
    void remonGlobalUnlock() {};
    void activateAddressSeg(void *address);

    bool remoteSwap(pod *pPod);

    bool activatePod(pod *podOfInterest);

    void freeLoopSignal();

    RamWatch * checkTidMappedFreeThread(double);

    void memoryFreeLoopSpl(RamWatch *ramWatchThreadSpecific);

    void setFreeOnFooter(pod *podOfInterest);

    void verifyLRUList();

    static bool hasDuplicates(const list<pod *> &myList);

    void makeSureAllPodsInThisHugeAllocatedBlockIsNotSwapped(pod *podOfInterest);

    mutex snapShotLock;
    void snapShot();
    size_t snapShotI = 0;

    void checkInitLRU();

    void asyncPromote(pod *podOfInterest);

    int tailPodInLRUList(pod *POI);

    int removePodFromLRU(pod *podOfInterest);

    void asyncSwapOutPod(pod *pPod);

    void remonPromoteRangeAsync(void* from, void* to);

    int remonPromotePod(pod *pPod);

    void remonForceSwapRangeAsync(void *from, void *to, void *exclude);

    void remonForceUnmapAsync(void *from, void *to, void *exclude);

    void remonDemoteRangeAsync(void *from, void *to, void *exclude);

    int remonPromoteAsync(void *addr);

    int getNcpu();

    inline void mapForPodI(int i);

public:
    remon_vmm();
    ~remon_vmm();

    static remon_vmm* remonSelfPtr;
    static void segfaultHandler(int signal, siginfo_t *si, void *arg);

    void* remonMalloc(size_t size);
    void remonFree(void* addr);
    void* remon_malloc(size_t size);
    void remon_free(void* addr);

    int remonPromote(void *addr); // 0 means it is head already NO-OP, 1 means pulled to head and 2 means fetching from remote async, -1 is error
    int remonPromoteRange(void* addrFrom, void* addrTo);
    int remonDemote(void *addr); // 0 means it is already on remote, 1 means it is local but put it down in the list
    int remonDemoteRange(void *addrFrom, void *addrTo, void *exclude);
    int remonForceSwap(void* addr);
    int remonForceSwapRange(void *addrFrom, void *addrTo, void *exclude);

    int remonForceUnmap(void* addrFrom, void* addrTo, void* exclude);

    //===--------------------------------------------------------------------===//
    // Synchronous APIs for DuckDB Buffer Manager Integration
    //===--------------------------------------------------------------------===//
    //! Synchronously activate a pod (fetch from remote/disk if needed)
    //! Blocks until the pod data is available in local memory
    //! Returns 1 on success, 0 if already local, -1 on error
    int remonActivateSync(void *addr);

    //! Clean up a pod that may be in remote/disk state before freeing
    //! This should be called before remon_free when a block was demoted
    //! Transitions pod to init state, cleaning up remote/disk resources
    void remon_cleanup_evicted(void *addr);

    double getMaxPM() {
        return remonConfig.maxRamInGB;
    }

    size_t getPageSize() {
        return remonConfig.pageSize;
    }

    uint64_t getPfnFromAddress(void* virtualAddress) {
        const int pageSize = getpagesize();

        uintptr_t addressValue = reinterpret_cast<uintptr_t>(virtualAddress);
        off_t offset = (addressValue / pageSize) * sizeof(uint64_t);

        std::string pagemapPath = "/proc/self/pagemap";

        std::ifstream pagemapFile(pagemapPath, std::ios::binary);

        if (!pagemapFile) {
            std::cerr << "Failed to open pagemap file." << std::endl;
            exit(1);
        }

        pagemapFile.seekg(offset);

        uint64_t PFN;
        pagemapFile.read(reinterpret_cast<char*>(&PFN), sizeof(uint64_t));

        return PFN;
    }

    uint64_t getFlags(uint64_t pfn) {
        pid_t pid = getpid();

        std::string flagPath = "/proc/kpageflags";

        off_t offset = pfn * sizeof(uint64_t);

        std::ifstream flagFile(flagPath, std::ios::binary);
        if (!flagFile) {
            std::cerr << "Failed to open kpageflags file." << std::endl;
            exit(1);
        }

        flagFile.seekg(offset);

        unsigned long long flags;
        flagFile.read(reinterpret_cast<char*>(&flags), sizeof(uint64_t));

        return flags;
    }

    bool isAPageActive(pod* POI) {
        size_t active = 0;
        size_t notActive = 0;
        size_t pageSize = POI->getSize();
        void* baseAddr = POI->getPayload();
        size_t size = 0;
        while(size < pageSize) {
            void* curr = pointerPlusOffset(baseAddr, sizeof(uint64_t));
            uint64_t flag = getFlags(getPfnFromAddress(curr));
            bool set = (flag >> 5) & 1;
            if(set) {
                active++;
            } else {
                notActive++;
            }
            size += sizeof(uint64_t);
        }
        cout << "count: " << active << " " << notActive << endl;
        cout << "ratio: " << (active * 1.0) / (active + notActive) << endl;
        if(active > notActive) {
            return true;
        } else {
            return false;
        }
    }

    void markLastAFewPageAsPreswap();

    void prefetch(pod *pPod, size_t knownPrefetchSize);

    void activatePodWrap(pod *, size_t knownPrefetch);

    void prefetchPod(pod *pPod);

    void reportPrefetcherStatistics();

};

using RemonVmm = remon_vmm;

#endif //REMON_H
