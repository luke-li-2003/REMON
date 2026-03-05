#ifndef REMON_POD_H
#define REMON_POD_H
#include <sys/mman.h>
#include "log.h"
#include "define.h"
#include "unistd.h"
#include <filesystem>
#include "mutex"
#include <vector>
#include "helperFunctions.h"
#include "profiler.h"
#include <fcntl.h>
#include <list>

class RemoteConnectionBundle;
#include "FreeListAllocator.h"
#include "RemoteConnectionBundle.h"

using namespace std;
class pod : public log {
private:
    volatile podState::podStateT state = podState::init;
    size_t size;
    void* payload;
    string swapPath;
    mutex mtx;
    int index;
    volatile int fd;

    RemoteConnectionBundle* landlordClient = nullptr;

    void map();
    int releaseMemory();

    FreeListAllocator allocator;

    vector<void*> freeBuffer;
    volatile bool isFlushing = false;

public:
    pod(size_t size, void *payload, int index, int fd);
    ~pod();
    // state change functions
    bool initToLocal();
    bool initToLocalPinned();
    bool localToInit();
    void localToLocalPinned() {
        if(getState() == podState::podStateT::local) {
            setState(podState::podStateT::localPinned);
        } else {
            stringstream ss;
            warn(ss << "illegal local->local_pinned, src not local: " << getState());
        }
    }
    void localPinnedToLocal() {
        if(getState() == podState::podStateT::localPinned) {
            setState(podState::podStateT::local);
        } else {
            stringstream ss;
            warn(ss << "illegal local->local_pinned, src not local_pinned: " << getState());
        }
    }
    void localToDisk(const string& path);
    void localToRemote(RemoteConnectionBundle *pClient);
    bool remoteToLocalPinned();

    bool preswap = false;
    //size_t rankCache = 0;
    void localToPreswap();
    void preswapToLocal();


    // get fd to send to remote
    int getI() const{return index;};
    void* getPayload() {return payload;}
    podState::podStateT getState(){
        auto snap = state;
        return snap;
    };
    void setState(podState::podStateT newState) {
        state = newState;
    }
    string getSwapPath() {
        return swapPath;
    }

    void lock(string reason){
#ifdef LOCK_PRINT
        stringstream ss;
        debug(ss << reason << " pod mux_lock: " << getI());
#endif
        mtx.lock();
#ifdef LOCK_PRINT
        debug(ss << reason << " pod locked: " << getI());
#endif
    }
    void unlock(string reason){
#ifdef LOCK_PRINT
        stringstream ss;
        debug(ss << reason << " pod unlock: " << getI());
#endif
        mtx.unlock();
#ifdef LOCK_PRINT
        debug(ss << reason << " pod unlocked: " << getI());
#endif
    }
    bool tryLock(string reason) {
#ifdef LOCK_PRINT
        stringstream ss;
        debug(ss << reason << " pod try mux_lock: " << getI());
#endif
        return mtx.try_lock();
    }

    void debugPrintBlocks() {
        allocator.debugPrint("external debug");
    }

    // pod level allocation code
    void* podMalloc(size_t requestedSize);
    void podFree(void* addr);

    // for LRU list
    time_t epoch = 1;
    void markLRU();
    bool LRUIterIsValid = false;
    list<pod *>::iterator LRUIter;

    // for spl
    // bool SPLIterIsValid = false;
    list<pod *>::iterator SPLIter;

    // for large free used
    pod* prev = nullptr;
    pod* next = nullptr;
    size_t globalSize = 0;
    bool isFree = true;
    pod* headPod = nullptr;

    size_t maxAllocatableSize(string reason);
    
    bool isForSmallAllocations();
    double getInternalFragmentation();

    void flushFreeBuffer();

    void touchFirstByte();

    bool addToFreeBufferReturnIfFull(void* ptr) {
        freeBuffer.push_back(ptr);
        if(freeBuffer.size() >= MAX_FREE_BUFF_SIZE) {
            unlock("add_to_free_buffer_return_if_full");
            return true;
        }
        return false;
    }

    bool diskToLocalPinned();

    long checkRSS();

    void podFreeNoLock(void *addr);

    void localToDiskUsingFork(const string &path);

    bool diskToLocalPinnedOldWay();

    void touchAllBytes();

    void recoverMemory();

    void unmap();

    void remapFd();

    bool toReadOnly();

    void toNone();

    void toReadAndWrite();

    void touchAllBytesCore();

    void* splPushdown = reinterpret_cast<void *>(0xDEADBEEF);

    void rememberTheIterToLRUList(list<pod *>::iterator iter);

    void diskToInit();

    void remoteToInit();

    double usageRatio();

    size_t getSize();

    void printRlimit();

    bool isEmpty();

    size_t getTailOffset();

    void * getPayloadEndMinusTwo();

    size_t sentOffset = 0;

    char* debugTail = nullptr;

    void toInit();

    void heapInit();

    int getFreeBufferSize();

    void useFd();

    void diskToDiskCache();

    void diskCacheToLocalPinned();

    void diskCacheToDisk();

    void remoteToRemoteCache();

    void remoteCacheToLocalPinned();

    void remoteCacheToRemote();

    void dropMemoryStayLocal(bool isHuge);

    void heapInitForce();

    size_t rank = -1;
    size_t oldRank = -1;
    bool updateRank() {
        rank = maxAllocatableSize("update_rank");
        if(rank != oldRank) {
            oldRank = rank;
            return true;
        }
        return false;
    }
};

#endif //REMON_POD_H
