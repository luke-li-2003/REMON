#ifndef REMON_DEFINE_H
#define REMON_DEFINE_H

#define GB 1073741824
#define M1 1048576
#define M2 2097152
#define M3 6291456
#define M4 4194304
#define M8 8388608
#define M16 16777216
#define M32 33554432
#define M64 67108864
#define M128 134217728
#define M256 268435456
#define M512 536870912
#define DEFAULT_PAGE_SIZE 851968
#define REMON_BUFFER_SIZE 1024
// #define POD_SIZE M8
#define MAX_RAM_DEFAULT 16
#define MAX_VM_DEFAULT 64
#define SHM_KEY 595699
#define PORT 50002
#define IO_RATE 40960000
#define K4 4096
#define CLIENT_TIMEOUT 6000
#define MAX_BIND_RETRY 100
#define IPC_SIZE 4096
#define FD_CONST 128
#define MIN_FD_COUNT 1024
#define MAX_FREE_BUFF_SIZE 1024
#define END_OF_FD (-2)
#define BUFF_POD 64
#define PERF 1
#define HARDCODE_SWAP_THREAD_COUNT 24
#define SOCKET_BEAM_SIZE 24
#define LATENCY_PANELTY 1000
#define USE_VALGRIND 1
#define MAX_PREFETCH 8
#define MAX_PREFETCH_S 0
#define PREFETCH 1
#define PREFETCH_DELAY 1
#define FREE_FLUSH_SKIP 0

// #define PRE_SWAP 1

//#define PAGE_COUNT 1024
#define PAGE_COUNT 1048576

namespace remoteFunction {
    typedef enum remoteFunction {
        invalid,
        pidFdRegisterInit,
        pidFdRegisterInProgress,
        pidFdRegisterOK,
        getMaxPodsInit,
        getMaxPodsOK,
        savePodInit,
        savePodOK,
        savePodErr,
        savePodDONE,
        getPodInit,
        getPodOK,
        getPodReady,
        getPodErr,
        getPodDONE,
        freePodInit,
        freePodOk,
        contextInit,
        contextInitOk,
        contextRegister,
        contextRegisterOk,
        getPodCacheInit,
        getPodCacheOK,
        getPodCacheReady,
        getPodCacheErr,
        getPodCacheDONE,
    } remoteFunctionT;
}

namespace podState {
    typedef enum podState {
        init,               // allocatable
        local,              // unallocatable and in memory
        localPinned,        // no_swap
        disk,               // server state, path is valid to rw
        remote,             // records where to find the page from remote via socket
        // preswap          // a quick recoverable state, mapped not read/writable but not deleted in memory
        diskCache,          // mapped with valid data but no rw permission, backed by remote memory/disk, may drop any time
        remoteCache
    } podStateT;
}

namespace ipc {
    typedef enum ipc {
        null,
        mapAddressByBaseOffset,
        mapAddressByBaseOffsetAck,
        unmapAddressByBaseOffset,
        unmapAddressByBaseOffsetAck,
        lock,
        unlock,
        shutdown
    } ipcT;
}

#endif //REMON_DEFINE_H
