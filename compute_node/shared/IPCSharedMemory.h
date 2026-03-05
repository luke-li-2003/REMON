#ifndef REMON_IPC_SHARED_MEMORY_H
#define REMON_IPC_SHARED_MEMORY_H

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <fcntl.h>

#include "log.h"
#include <semaphore.h>

using namespace std;
class IPCSharedMemory : public log {

private:
    void* totalIPCRegion = nullptr;
    char* bufferRegion;
    int fd;
    sem_t* semaphore;
    volatile ipc::ipcT* flag;
    long usableBufferSize;
    void initSemaphore(bool isActive);
    bool isMine;

public:
    IPCSharedMemory(bool isMine);
    IPCSharedMemory(pid_t pid, int guestFd, bool isMine);
    ~IPCSharedMemory();

    int guestFdToLocalFd(pid_t guestPid, int guestFd);
    void IPCReset();
    
    void wait();
    void post();

    void setFlag(ipc::ipcT newValue) {
        *flag = newValue;
        sync();
    }
    
    int getFd(){return fd;}
    ipc::ipcT getFlag(){sync(); return *flag;}
    char* getBufferRegion(){return bufferRegion;}
    void* getTotalIPCRegion(){return totalIPCRegion;};

    void sync() {
        /*int result = fsync(fd);
        if(result) {
            stringstream ss;
            err(ss << "fsync failed");
        }*/
    }

};

#endif //REMON_IPC_SHARED_MEMORY_H
