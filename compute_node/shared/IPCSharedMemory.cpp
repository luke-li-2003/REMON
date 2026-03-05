#include "IPCSharedMemory.h"

IPCSharedMemory::IPCSharedMemory(bool isMine) : log("IPC_shared_memory") {
    stringstream ss;
    fd = memfd_create("default", 0);
    if (fd < 0) {
        err(ss << "failed to create fd");
        fd = -1;
        return;
    }

    size_t size = IPC_SIZE;
    int check = ftruncate(fd, size);
    if (check == -1) {
        err(ss << "failed to size the fd to " << size << "bytes");
        fd = -1;
        return;
    }

    totalIPCRegion = (uint8_t *) mmap(nullptr, IPC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (totalIPCRegion == MAP_FAILED) {
        err(ss << "shared mmap failed");
        fd = -1;
        return;
    }
    debug(ss << "IPC region bonded to fd (active): " << fd << " " << reinterpret_cast<void*>(totalIPCRegion));

    initSemaphore(true);
    if(isMine) {
        wait();
    }
    this->isMine = isMine;
}

// create IPC passively to correspond with incoming IPC request
IPCSharedMemory::IPCSharedMemory(pid_t pid, int guestFd, bool isMine) : log("IPC_shared_memory") {
    stringstream ss;
    this->fd = guestFdToLocalFd(pid, guestFd);
    totalIPCRegion = (uint8_t *) mmap(nullptr, IPC_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
    if (totalIPCRegion == MAP_FAILED) {
        err(ss << "Failed to allocate vm, need to exit");
        exit(-1);
    }

    debug(ss << "Passively mapped IPC region: " << reinterpret_cast<void*>(totalIPCRegion));
    initSemaphore(false);
    if(isMine) {
        wait();
    }

    this->isMine = isMine;
}

IPCSharedMemory::~IPCSharedMemory() {
    stringstream ss;
    debug(ss << "munmap total_IPC_region: " << reinterpret_cast<void*>(totalIPCRegion));

    if(isMine) {
        sem_post(semaphore);
        sem_destroy(semaphore);
    }

    munmap(totalIPCRegion, IPC_SIZE);
    close(fd);
}

void IPCSharedMemory::initSemaphore(bool isActive) {
    stringstream ss;
    semaphore = (sem_t *) totalIPCRegion;
    
    if (isActive) {
        int ret = sem_init(semaphore, 1, 1);
        if (ret) {
            err(ss << "sem_init errno: " << strerror(errno));
        }
    }

    semaphore = (sem_t *) totalIPCRegion;
    int lockSize = sizeof(sem_t);
    flag = (ipc::ipcT *) ((u_int8_t *) semaphore + lockSize);
    bufferRegion = (char *) flag + sizeof(ipc::ipcT);

    debug(ss << "ipc::ipc_t flag size is " << sizeof(ipc::ipcT));
    debug(ss << "mux_lock is placed: " << reinterpret_cast<void *>(semaphore) << " buffer_region: "
            << reinterpret_cast<void *>(bufferRegion));

    usableBufferSize = (u_int8_t *) totalIPCRegion + IPC_SIZE - (u_int8_t *) bufferRegion;
    
    debug(ss << "mux_lock init OK: IPC buffer_region is behind total_IPC_region by: "
            << (u_int8_t *) bufferRegion - (u_int8_t *) totalIPCRegion << " max buffer size is "
            << usableBufferSize);
}

int IPCSharedMemory::guestFdToLocalFd(pid_t guestPid, int guestFd) {
    char fdPath[64];
    snprintf(fdPath, sizeof(fdPath), "/proc/%d/fd/%d", guestPid, guestFd);
    int localFd = open(fdPath, O_RDWR);
    return localFd;
}


void IPCSharedMemory::wait() {
    int ret = sem_wait(semaphore);
    if (ret) {
        /*if(ret == EOWNERDEAD) {
            pthread_mutex_consistent(mux_lock);
            return;
        }*/
        stringstream ss;
        err(ss << "wait errno: number:" << ret << " str:" << strerror(ret));
    }
}

void IPCSharedMemory::post() {
    int ret = sem_post(semaphore);
    if (ret) {
        stringstream ss;
        err(ss << "post errno: number:" << ret << " str:" << strerror(ret));
    }
}

void IPCSharedMemory::IPCReset() {
    stringstream ss;
    debug(ss << "IPC_reset\n");

    *flag = ipc::ipcT::null;
    memset(bufferRegion,0,usableBufferSize);
}
