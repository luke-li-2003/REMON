#ifndef REMON_FREE_LIST_ALLOCATOR_H
#define REMON_FREE_LIST_ALLOCATOR_H

#include "log.h"
#include "helperFunctions.h"
#include "allocatorMacros.h"
#include "cassert"
#include "bitset"
#include "mutex"


using namespace std;
class FreeListAllocator : public log {
private:
    size_t payloadSize;
    size_t wordSize;
    void* freeHead = nullptr;

    mutex lock;
    size_t MAX_INTRA_POD_SIZE;
public:
    uint8_t* payloadStart;
    uint8_t* payloadEnd;
    volatile bool inited = false;

    void checkAlignment();

    void heapInit();

    FreeListAllocator(void* start, void* end) : log("free_list_allocator"){
        stringstream ss;
        payloadStart = (uint8_t*) start;
        payloadEnd = (uint8_t*) end;
        payloadSize = payloadEnd - payloadStart;
        wordSize = payloadSize / WSIZE;
        checkAlignment();
        MAX_INTRA_POD_SIZE = payloadSize - QSIZE;
    };

    void* malloc(size_t requestedSize);
    void free(void* addr);
    void coalescing(void*);
    void leftCoalescing(void *rootBp);
    static void popFreeBlockOut(void *bp);
    size_t computeAsize(size_t size);
    void memDump();
    void freeListDump(string);

    void insertAtFreeHead(void *bp);
    bool isInited() {
        return inited;
    }

    size_t maxAllocatableSize();

    size_t getBlockSizeHelper(void *ptr);

    void *getNetHelper(void *ptr);

    void debugPrint(string);

    void rightCoalescing(void *pVoid);

    double getFragmentation();

    bool isEmpty() {
        if(freeHead) {
            if (GET_BLOCK_SIZE(freeHead) == payloadSize - DSIZE) {
                return true;
            }
        }
        return false;
    }
    size_t getTailFreeBlock() {
        void* footerLocation = payloadEnd - DSIZE;
        
        if(GET_ALLOC(footerLocation) == 0) {
            void* bp = pointerPlusOffset(footerLocation, - (GET_SIZE(footerLocation) - DSIZE));
            return pointerMinusPointer(bp, payloadStart);
        }
        return payloadSize;
    }
};


#endif //REMON_FREE_LIST_ALLOCATOR_H
