#include "FreeListAllocator.h"
#include "profiler.h"

profiler podCoalescingProfiler("pod_coalescing_profiler");
profiler maxFreeProfiler("max_free_profiler");

void *FreeListAllocator::malloc(size_t requestedSize) {

    lock.lock();
    if(!inited) {
        heapInit();
    }
    if(requestedSize == 0) {
        return nullptr;
    }

    if(requestedSize > MAX_INTRA_POD_SIZE) {
        stringstream ss;
        err(ss << "oversize request should not go into intra allocator");
        return nullptr;
    }

    //size_t asize = compute_asize(requested_size);
    size_t asize = requestedSize;

    void* oldFreeHead = freeHead;
    if(freeHead != nullptr) {
        void* curr = freeHead;
        while(true) {
            int currBlockSize = GET_BLOCK_SIZE(curr);
            void* prev = (void*) GET(PREV_FREE(curr));
            void* next = (void*) GET(NEXT_FREE(curr));
            
            if (currBlockSize >= asize) {
                if(prev != nullptr) {
                    PUT(NEXT_FREE(prev), (intptr_t) next);
                }
                if(next != nullptr) {
                    PUT(PREV_FREE(next), (intptr_t) prev);
                }

                size_t residualSize = currBlockSize - asize;
                if(residualSize < QSIZE) {
                    asize = currBlockSize;
                    residualSize = 0;
                }
                SET_BLOCK_SIZE(curr, asize);
                SET_ALLOC(curr);
                if(GET_ALLOC(HDRP(curr)) == 0) {
                    cout << "ALLOC is not working" << endl;
                    exit(-2);
                }

                if(residualSize != 0) {
                    void *newFreeSegment = NEXT_BLKP(curr);
                    if(newFreeSegment == curr) {
                        stringstream ss;
                        err(ss << "new_free_segment == curr allocated");
                    }
                    SET_BLOCK_SIZE(newFreeSegment, residualSize);
                    SET_FREE(newFreeSegment);

                    insertAtFreeHead(newFreeSegment);
                }
                lock.unlock();

                return curr;
            }
            if (next == nullptr) {
                break;
            }
            curr = next;
        }
    }
    lock.unlock();

    if(freeHead == nullptr && oldFreeHead != nullptr) {
        stringstream ss;
        err(ss << "free_head turns to null during malloc failed");
        exit(-2);
    }
    return nullptr;
}

void FreeListAllocator::free(void *addr) {
    stringstream ss;
    lock.lock();
    SET_FREE(addr);
    insertAtFreeHead(addr);
    podCoalescingProfiler.record();
    coalescing(addr);
    podCoalescingProfiler.writeRecord();
    lock.unlock();
    if(freeHead == nullptr) {
        err(ss << "free_head turns to null during free");
        exit(-2);
    }
}

void FreeListAllocator::checkAlignment() {
    stringstream ss;
    size_t mask = 0b111;
    size_t result = (size_t) payloadStart & mask;
    if (result != 0) {
        err(ss << "payload_start is not WORD aligned");
    }
}

void FreeListAllocator::heapInit() {
    if(inited) {
        return;
    }

    stringstream ss;
    PUT(payloadStart, 1);
    int largestSize = MAX_INTRA_POD_SIZE;
    int blockSize = largestSize + OVERHEAD;
    uint8_t* blockPointer = payloadStart + DSIZE;

    SET_BLOCK_SIZE(blockPointer, blockSize);
    if(GET_BLOCK_SIZE(blockPointer) != blockSize) {
        err(ss << "size initialization failed: should be " << blockSize << " we have " << GET_SIZE(blockPointer));
    }
    SET_FREE(blockPointer);

    freeHead = blockPointer;
    PUT(payloadEnd - WSIZE, 1);
    PUT(PREV_FREE(blockPointer), (intptr_t) &freeHead);
    PUT(NEXT_FREE(blockPointer), 0);

    inited = true;
    if(freeHead == nullptr) {
        err(ss << "heap_init but free_head == nullptr");
        exit(-2);
    }
}

size_t FreeListAllocator::computeAsize(size_t size) {
    size_t asize;
    if(size <= DSIZE) {
        asize = DSIZE + OVERHEAD;
    } else {
        asize = DSIZE * ((size + OVERHEAD + DSIZE - 1) / DSIZE);
    }
    return asize;
}

void FreeListAllocator::memDump() {
    uint8_t* cursor = payloadStart;
    for(int i = 0; i < wordSize; i++) {
        cout << GET(cursor) << " ";
        if(i % 32 == 0 && i != 0) {
            cout << endl;
        }
        cursor += WSIZE;
    }
    cout << endl;
}

void FreeListAllocator::insertAtFreeHead(void *bp) {
    PUT(NEXT_FREE(bp), (intptr_t) freeHead);
    if(freeHead) {
        PUT(PREV_FREE(freeHead), (intptr_t) bp);
    }

    freeHead = bp;
    PUT(PREV_FREE(bp), (intptr_t) &freeHead);

    if(freeHead == nullptr) {
        stringstream ss;
        err(ss << "free_head is null after the insert_at_free_head");
        exit(-2);
    }
}

void FreeListAllocator::freeListDump(string reason) {
    stringstream ss;
    void* curr = freeHead;
    int i = 0;

    while(curr) {
        int currBlockSize = GET_BLOCK_SIZE(curr);
        void* next = (void*) GET(NEXT_FREE(curr));
        cout << reason << ": Free block #" << i << ":" << currBlockSize << " @ " << curr << " prev: " <<
            (void*) PREV_FREE_VAL(curr) << " next: " <<(void*) NEXT_FREE_VAL(curr) << endl;
        if (next == nullptr) {
            info(ss << "free_list_dump: reached end of the freelist");
            break;
        }
        curr = next;
        i++;
    }
}

void FreeListAllocator::coalescing(void* bp) {
    rightCoalescing(bp);
    stringstream ss;
    if(freeHead == nullptr) {
        err(ss << "failed coalescing, caused free_head to be null");
        exit(-2);
    }
}

void FreeListAllocator::leftCoalescing(void *rootBp) {
    if(rootBp < payloadStart || rootBp > payloadEnd) {
        return;
    }
    void* currBp = rootBp;
    size_t freeSize = 0;
    while(true) {
        if((char*)(currBp) - DSIZE < (char*) payloadStart) {
            break;
        }
        size_t prevAlloc = GET_ALLOC((char*)(currBp) - DSIZE);
        if(prevAlloc) {
            break;
        } else {
            popFreeBlockOut(currBp);
            size_t size = GET_BLOCK_SIZE(currBp);
            freeSize += size;
            currBp = PREV_BLKP(currBp);
        }
    }
    
    if(freeSize) {
        SET_BLOCK_SIZE(currBp, freeSize + GET_BLOCK_SIZE(currBp));
        SET_FREE(currBp);
    }
}

void FreeListAllocator::popFreeBlockOut(void *bp) {
    void* prev = (void*) GET(PREV_FREE(bp));
    void* next = (void*) GET(NEXT_FREE(bp));

    if(prev != nullptr) {
        PUT(NEXT_FREE(prev), (intptr_t) next);
    }
    if(next != nullptr) {
        PUT(PREV_FREE(next), (intptr_t) prev);
    }
}

size_t FreeListAllocator::getBlockSizeHelper(void* ptr) {
    if(ptr < payloadStart || ptr > payloadEnd) {
        cout << "ptr sniff: " << ptr << endl;
    }
    return GET_BLOCK_SIZE(ptr);
}

void* FreeListAllocator::getNetHelper(void* ptr) {
    return (void*) GET(NEXT_FREE(ptr));
}

size_t FreeListAllocator::maxAllocatableSize() {
    if(!inited) {
        heapInit();
    }
    
    void* curr = freeHead;
    size_t maxSize = 0;
    while(curr) {
        size_t currBlockSize = GET_BLOCK_SIZE(curr);
        if(currBlockSize > maxSize) {
            maxSize = currBlockSize;
        }

        void* next = (void*) GET(NEXT_FREE(curr));
        if (next == nullptr) {
            break;
        }
        curr = next;
    }
    return maxSize;
}

void FreeListAllocator::debugPrint(string reason) {
    stringstream ss;

    void* curr = freeHead;
    size_t freeListFreeSize = 0;
    while(curr) {
        size_t currBlockSize = GET_BLOCK_SIZE(curr);
        freeListFreeSize += currBlockSize;
        void* next = (void*) GET(NEXT_FREE(curr));
        if (next == nullptr) {
            break;
        }
        curr = next;
    }

    uint8_t* firstBlock = payloadStart + DSIZE;
    size_t freeBlockSizeSum = 0;
    size_t allocatedBlockSizeSum = 0;
    curr = firstBlock;
    while(curr) {
        size_t currBlockSize = GET_BLOCK_SIZE(curr);

        if(GET_ALLOC(HDRP(curr))) {
            allocatedBlockSizeSum += currBlockSize;
        } else {
            freeBlockSizeSum += currBlockSize;
        }
        void* next = NEXT_BLKP(curr);
        if(next > payloadEnd) {
            err(ss << reason << ": exceeded limit: " << reinterpret_cast<void*>(next) << " vs " << reinterpret_cast<void*>(payloadEnd));
            break;
        }
        if(next == payloadEnd) {
            break;
        }
        if (next == nullptr) {
            break;
        }
        curr = next;
    }

    if(freeBlockSizeSum + allocatedBlockSizeSum != payloadSize - OVERHEAD) {
        err(ss << reason << ": miss match blocks: " << payloadSize - OVERHEAD);
    }
    if(freeListFreeSize != freeBlockSizeSum) {
        err(ss << reason << ": miss match free blocks");
    }
}

void FreeListAllocator::rightCoalescing(void *rootBp) {
    if(rootBp < payloadStart || rootBp >= payloadEnd) {
        return;
    }
    void* currBp = rootBp;
    size_t freeSize = 0;
    vector<void*> bpsToRemove;
    while(true) {
        void* nextBp = NEXT_BLKP(currBp);
        if(nextBp >= payloadEnd) {
            break;
        }
        size_t nextAlloc = GET_ALLOC(HDRP(nextBp));
        if(nextAlloc) {
            break;
        } else {
            bpsToRemove.push_back(nextBp);
            size_t size = GET_BLOCK_SIZE(nextBp);
            freeSize += size;
            currBp = nextBp;
        }
    }
    
    if(freeSize) {
        SET_BLOCK_SIZE(rootBp, freeSize + GET_BLOCK_SIZE(rootBp));
        SET_FREE(rootBp);
    }
    for(auto bp:bpsToRemove) {
        popFreeBlockOut(bp);
    }
}

double FreeListAllocator::getFragmentation() {
    uint8_t* firstBlock = payloadStart + DSIZE;
    size_t freeBlockSizeSum = 0;
    size_t allocatedBlockSizeSum = 0;
    void* curr = firstBlock;
    
    while(curr) {
        size_t currBlockSize = GET_BLOCK_SIZE(curr);
        if(GET_ALLOC(HDRP(curr))) {
            allocatedBlockSizeSum += currBlockSize;
        } else {
            freeBlockSizeSum += currBlockSize;
        }
        void* next = NEXT_BLKP(curr);
        if(next < payloadStart) {
            stringstream ss;
            warn(ss << "NEXT_BLKP points before payload");
            break;
        }
        if(next >= payloadEnd) {
            break;
        }
        if (next == nullptr) {
            break;
        }
        if(curr == next) {
            stringstream ss;
            warn(ss << "curr == next inside get_fragmentation");
            break;
        }
        curr = next;
    }
    // if(freeBlockSizeSum + allocatedBlockSizeSum != payloadSize - OVERHEAD) {
    // }

    return freeBlockSizeSum*1.0/(payloadSize-OVERHEAD);
}



