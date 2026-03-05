#include <bits/types/siginfo_t.h>
#include <sys/wait.h>
#include "pod.h"
#include "RemoteConnectionClient.h"

pod::pod(size_t size, void *payload, int index, int fd) :
            log("pod fd=" + to_string(index)),
            allocator(payload, pointerPlusOffset(payload, size)){
    this->size = size;
    this->payload = payload;
    this->index = index;
    this->fd = fd;
}

bool pod::initToLocal() {
    useFd();
    setState(podState::podStateT::local);
    if(mprotect(payload, size, PROT_READ | PROT_WRITE) != 0) {
        stringstream ss;
        err(ss << "init -> local state change failed");
        return false;
    }
#ifdef PRINT_STATE_CHANGE
    stringstream ss;
    info(ss << "init -> local state success: i = " << index);
#endif
    markLRU();
    return true;
}

bool pod::initToLocalPinned() {
    useFd();
    setState(podState::podStateT::localPinned);
    if(mprotect(payload, size, PROT_READ | PROT_WRITE) != 0) {
        stringstream ss;
        err(ss << "init -> local state change failed");
        return false;
    }
#ifdef PRINT_STATE_CHANGE
    stringstream ss;
    info(ss << "init -> local state success: i = " << index);
#endif
    markLRU();
    return true;
}

void pod::map() {
    void* requestedAddr = payload;
    if(fd != -1) {
        payload = (uint8_t *) mmap(payload, size, PROT_NONE, MAP_SHARED | MAP_FIXED, fd, 0);
    } else {
        payload = (uint8_t *) mmap(payload, size, PROT_NONE, MAP_ANONYMOUS| MAP_PRIVATE | MAP_FIXED, -1, 0);
    }
    if(!payload) {
        stringstream ss;
        err(ss << "Failed to allocate vm, need to exit");
        exit(-1);
    }
    if(requestedAddr != payload) {
        stringstream ss;
        perror("mmap");
        fprintf(stderr, "mmap failed: %d\n", errno);
        printRlimit();
        err(ss << "payload using unexpected address: " << payload << " fd: " << fd << " i: " << index);
        exit(-1);
    }
}

bool pod::localToInit() {
    setState(podState::podStateT::init);
    releaseMemory();
    recoverMemory();
    swapPath = "";
#ifdef PRINT_STATE_CHANGE
    stringstream ss;
    info(ss << "local -> init state success: i = " << index);
#endif
    allocator.inited = false;
    return true;
}

void pod::localToDisk(const string& path) {
    stringstream ss;

    if(state != podState::podStateT::local) {
        err(ss << "local_to_disk but state OG was not local: " << state);
    }
    setState(podState::podStateT::disk);
    toReadOnly();

    ofstream fh;
    fh.open(path, std::ios::out | std::ios::binary);
    if (!fh.is_open()) {
        err(ss << "swap_path create file failed: " << getI());
        exit(1);
    }

    size_t tailOffset = getTailOffset();
    sentOffset = tailOffset;
    fh.write((char*) payload, (long) tailOffset);
    if(tailOffset != size) {
        fh.write((char*) payload + tailOffset, DSIZE);
        fh.write((char*) payload + size - DSIZE, DSIZE);
    }
    fh.flush();
    fh.close();
    toNone();
    releaseMemory();
    recoverMemory();
    swapPath = path;
#ifdef PRINT_STATE_CHANGE
    info(ss << "local -> disk state success: i = " << index);
#endif
}

int pod::releaseMemory() {
    int check = ftruncate(fd, 0);
    if (check == -1) {
        stringstream ss;
        err(ss << "failed to size the fd to " << size << " bytes");
        perror(("failed to size the fd to " + to_string(size)).c_str());
        return -1;
    }
    return 0;
}

bool pod::diskToLocalPinned() {
    stringstream ss;
    void* copyMap = mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);

    ifstream fh(swapPath, ios::in | ios::binary);
    if(!fh.is_open()) {
        err(ss << "swap_path has no file: " << getI() << " path: " << swapPath);
        exit(1);
    }

    fh.read(reinterpret_cast<char*>(copyMap), sentOffset);
    if(sentOffset != size) {
        fh.read(reinterpret_cast<char*>(copyMap) + sentOffset, DSIZE);
        fh.read(reinterpret_cast<char*>(copyMap) + size - DSIZE, DSIZE);
    }
    fh.close();

    filesystem::remove(swapPath);
    swapPath = "";

    toReadAndWrite();
    touchAllBytes();
    munmap(copyMap, size);

    markLRU();
    flushFreeBuffer();

    setState(podState::podStateT::localPinned);
#ifdef PRINT_STATE_CHANGE
    info(ss << "disk -> local_pinned state success: i = " << index);
#endif
    return true;
}


pod::~pod() {
    stringstream ss;
    if(!swapPath.empty()) {
        filesystem::remove(swapPath);
    }
    if(landlordClient) {
        // debug(ss << "landlord_client free: " << landlord_client);
        // landlord_client->free_pod_in_remote(this);
    }
}

#include <cmath>
void *pod::podMalloc(size_t requestedSize) {
    if(state != podState::podStateT::local) {
        stringstream ss;
        warn(ss << "malloc and then the pod is not local: " << state);
        return nullptr;
    }
    void* ptr = allocator.malloc(requestedSize);
    
    return ptr;
}

void pod::podFree(void *addr) {
    lock("pod_free");
    podFreeNoLock(addr);
    unlock("pod_free");
}
void pod::podFreeNoLock(void *addr) {
    allocator.free(addr);
}


void pod::localToRemote(RemoteConnectionBundle *client) {
    setState(podState::podStateT::remote);
    releaseMemory();
    recoverMemory();

    landlordClient = client;
#ifdef PRINT_STATE_CHANGE
    stringstream ss;
    info(ss << "local -> remote state success: i = " << index << " ip = " << landlordClient->ipStr);
#endif

}

bool pod::remoteToLocalPinned() {
    stringstream ss;
    void* copyMap = mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);

    landlordClient->getPodFromRemote(this, copyMap);
    landlordClient = nullptr;

    int ret = munmap(copyMap,size);
    if(ret < 0) {
        perror("unmap duplicated mapping");
    }
    toReadAndWrite();
    touchAllBytes();

    markLRU();
    
    flushFreeBuffer();
    setState(podState::podStateT::localPinned);
#ifdef PRINT_STATE_CHANGE
    info(ss << "remote -> local state success: i = " << index);
#endif

    return true;
}

void pod::markLRU() {
}

size_t pod::maxAllocatableSize(string reason) {
    if(state != podState::podStateT::local && state != podState::podStateT::localPinned) {
        stringstream ss;
        warn(ss << "max_allocatable_size wrong state: " << state << " reason: " << reason);
    }

    size_t maxAllocatableSize = allocator.maxAllocatableSize();
    return maxAllocatableSize;
}

bool pod::isForSmallAllocations() {
    if(allocator.isInited()) {
        return true;
    } else {
        return false;
    }
}

double pod::getInternalFragmentation() {
    return allocator.getFragmentation();
}

void pod::flushFreeBuffer() {
    stringstream ss;
    if(isFlushing) {
        warn(ss << "free_buffer is_flushing");
        return;
    }
    if(freeBuffer.empty()) {
        return;
    }

    isFlushing = true;
    for(auto ptr:freeBuffer) {
        podFreeNoLock(ptr);
    }
    
    freeBuffer.clear();
    isFlushing = false;
}

long pod::checkRSS() {
    ifstream statmStream("/proc/self/statm");
    size_t sizeInPages;
    statmStream >> sizeInPages;
    statmStream >> sizeInPages;
    statmStream.close();
    return sizeInPages;
}

void pod::touchFirstByte() {
    stringstream ss;
    info(ss << "touching the first byte: " << reinterpret_cast<void*>(((char*)payload)[0]));
    char test = ((char*)payload)[0];
    string a(&"lol" [test]);
}

void pod::touchAllBytes() {
    touchAllBytesCore();
}

void pod::recoverMemory() {
    int check = ftruncate(fd, size);
    if (check == -1) {
        stringstream ss;
        err(ss << "failed to size the fd to " << size << " bytes");
        perror(("failed to size the fd to " + to_string(size)).c_str());
    }
}

void pod::unmap() {
    munmap(payload,size);
}

void pod::remapFd() {
    stringstream ss;
    int ret = close(fd);
    if(ret != 0) {
        err(ss << "close fd failed");
        perror("remap_fd: close(fd)");
    }
    debug(ss << "old fd: " << fd);
    fd = memfd_create("default", 0);
    debug(ss << "new fd: " << fd);
    if (fd < 0) {
        err(ss << "failed to create fd");
        perror("failed to create fd");
    }
    int check = ftruncate(fd, size);
    if (check != 0) {
        err(ss << "failed to size the fd to " << size << "bytes");
        perror("failed to size the fd");
    }
}

bool pod::toReadOnly() {
    if(mprotect(payload, size, PROT_READ) != 0) {
        stringstream ss;
        err(ss << "disable memory before remap failed");
        perror("to_read_only");
    }
    return true;
}

void pod::toNone() {
    if(mprotect(payload, size, PROT_NONE) != 0) {
        stringstream ss;
        err(ss << "disable memory before remap failed");
        perror("to_none");
    }
}

void pod::toReadAndWrite() {
    if(mprotect(payload, size, PROT_READ | PROT_WRITE) != 0) {
        stringstream ss;
        err(ss << "disk -> local_pinned state change failed");
        perror("to_read_and_write");
        exit(-1);
    }
}

void pod::touchAllBytesCore() {
    volatile int result = 1;
    for(size_t i = 0; i < sentOffset; i++) {
        result |= ((char*)payload)[i];
    }
}

void pod::rememberTheIterToLRUList(list<pod *>::iterator iter) { 
    LRUIter = iter;
    LRUIterIsValid = true;
}

void pod::diskToInit() {
    filesystem::remove(swapPath);
    stringstream ss;
    setState(podState::podStateT::init);
    releaseMemory();
    recoverMemory();
    swapPath = "";
    allocator.inited = false;
#ifdef PRINT_STATE_CHANGE
    info(ss << "disk -> init state success: i = " << index);
#endif
}

void pod::remoteToInit() {
    landlordClient->freeRemotePod(this);
    stringstream ss;
    setState(podState::podStateT::init);
    releaseMemory();
    recoverMemory();
    landlordClient = nullptr;
    allocator.inited = false;
#ifdef PRINT_STATE_CHANGE
    info(ss << "remote -> init state success: i = " << index);
#endif
}

double pod::usageRatio() {
    double result = 1.0 - allocator.getFragmentation();
    return result;
}

size_t pod::getSize() {
    return size;
}

void pod::printRlimit() {
    struct rlimit old;
    struct rlimit *newp;
    if (prlimit(getpid(), RLIMIT_DATA, newp, &old) == -1) {
        stringstream ss;
        err(ss << "failed to get RLIMIT_DATA");
    }
    printf("Previous limits: soft=%jd; hard=%jd\n",(intmax_t) old.rlim_cur, (intmax_t) old.rlim_max);
    cout << "Also check your /proc/sys/vm/max_map_count" << endl;
}

bool pod::isEmpty() {
    if(isForSmallAllocations()) {
        if(allocator.isEmpty()) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

size_t pod::getTailOffset() {
    if(isForSmallAllocations()) {
        return allocator.getTailFreeBlock();
    }
    return size;
}

void * pod::getPayloadEndMinusTwo() {
    return pointerPlusOffset(payload, size - DSIZE);
}

void pod::toInit() {
    switch(getState()) {
        case podState::podStateT::local:
            localToInit();
            break;
        case podState::podStateT::disk:
            diskToInit();
            break;
        case podState::remote:
            remoteToInit();
            break;
        default:
            break;
    }
}

void pod::localToPreswap() {
    toNone();
    preswap = true;
}

void pod::preswapToLocal() {
    toReadAndWrite();
    preswap = false;
}

void pod::heapInit() {
    if(!allocator.isInited()) {
        allocator.heapInit();
    }
}

int pod::getFreeBufferSize() {
    return freeBuffer.size();
}

void pod::useFd() {
    stringstream ss;
    if(fd == -2) {
        fd = memfd_create("default", 0);
        if (fd < 0) {
            err(ss << "failed to create fd i=" << index);
            perror("failed to create fd");
            exit(-1);
        }
        int check = ftruncate(fd, size);
        if (check == -1) {
            err(ss << "failed to size the fd to " << size << "bytes");
            perror("failed to size the fd");
            exit(-1);
        }
    }
    map();
}

void pod::diskToDiskCache() {
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk -> disk_cache state start: i = " << index);
    }
#endif
    void* copyMap = mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);
    if(copyMap == MAP_FAILED) {
        perror("map");
        std::cout << "disk_to_disk_cache: Also check your /proc/sys/vm/max_map_count" << std::endl;
        exit(-1);
    }
    ifstream fh(swapPath, ios::in | ios::binary);
    if(!fh.is_open()) {
        stringstream ss;
        err(ss << "swap_path has no file: " << getI() << " path: " << swapPath);
        exit(1);
    }
    fh.read(reinterpret_cast<char*>(copyMap), sentOffset);
    if(sentOffset != size) {
        fh.read(reinterpret_cast<char*>(copyMap) + sentOffset, DSIZE);
        fh.read(reinterpret_cast<char*>(copyMap) + size - DSIZE, DSIZE);
    }
    fh.close();
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk -> disk_cache state touch: i = " << index << " range: "
            << copyMap << " ~ " << pointerPlusOffset(copyMap, size));
    }
#endif

#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk -> disk_cache state touched: i = " << index);
    }
#endif
    setState(podState::podStateT::diskCache);
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk -> disk_cache state success: i = " << index);
    }
#endif
    return;
}

void pod::diskCacheToLocalPinned() {
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk_cache -> local_pinned start: i = " << index);
    }
#endif
    filesystem::remove(swapPath);
    swapPath = "";
    toReadAndWrite();
    flushFreeBuffer();
    setState(podState::podStateT::localPinned);
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk_cache -> local_pinned state success: i = " << index);
    }
#endif
}

void pod::diskCacheToDisk() {
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk_cache -> disk state start: i = " << index);
    }
#endif
    releaseMemory();
    recoverMemory();
    setState(podState::podStateT::disk);
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "disk_cache -> disk state success: i = " << index);
    }
#endif
}

void pod::remoteToRemoteCache() {
#ifdef PRINT_STATE_CHANGE
    {stringstream ss;
    info(ss << "remote -> remote_cache state start: i = " << index);}
#endif
    stringstream ss;
    void* copyMap = mmap(NULL, size, PROT_WRITE, MAP_SHARED | MAP_NORESERVE, fd, 0);
    landlordClient->getPodCacheFromRemote(this, copyMap);
    volatile int result = 1;
    for(size_t i = 0; i < sentOffset; i++) {
        result |= ((char*)copyMap)[i];
    }
    int ret = munmap(copyMap,size);
    if(ret < 0) {
        perror("unmap duplicated mapping");
    }
    setState(podState::podStateT::remoteCache);
#ifdef PRINT_STATE_CHANGE
    {stringstream ss;
    info(ss << "remote -> remote_cache state success: i = " << index);}
#endif

    return;
}

void pod::remoteCacheToLocalPinned() {
#ifdef PRINT_STATE_CHANGE
    {stringstream ss;
    info(ss << "remote_cache -> local_pinned state start: i = " << index);}
#endif
    landlordClient->freeRemotePod(this);
    landlordClient = nullptr;
    toReadAndWrite();
    flushFreeBuffer();
    setState(podState::podStateT::localPinned);
#ifdef PRINT_STATE_CHANGE
    {stringstream ss;
    info(ss << "remote_cache -> local_pinned state success: i = " << index);}
#endif

    return;
}

void pod::remoteCacheToRemote() {
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "remote_cache -> remote state start: i = " << index);
    }
#endif
    releaseMemory();
    recoverMemory();
    setState(podState::podStateT::remote);
#ifdef PRINT_STATE_CHANGE
    {
        stringstream ss;
        info(ss << "remote_cache -> remote state success: i = " << index);
    }
#endif
}

void pod::dropMemoryStayLocal(bool isHuge) {
    releaseMemory();
    recoverMemory();
    if(!isHuge) {
        heapInitForce();
    }
}

void pod::heapInitForce() {
    allocator.inited = false;
    allocator.heapInit();
}


