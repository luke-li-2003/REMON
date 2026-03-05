#ifndef REMON_POD_BOUNDED_BUFFER_H
#define REMON_POD_BOUNDED_BUFFER_H

#include "pod.h"
#include "log.h"
#include "TopLevelAllocator.h"
#include "TopLevelAllocatorBundle.h"

class PodBoundedBuffer : public log{
private:
    vector<pod*> buffer;
    int readIndex = 0;
    int writeIndex = 0;
    vector<mutex>* bufferLocks;
    condition_variable producerCv;
    condition_variable consumerCv;
    volatile int bufferThreadRunning = 0;
    thread* bufferKeeperThread = nullptr;
    
    mutex lock;
    TopLevelAllocatorBundle* tlaPushdown = nullptr;
public:
    PodBoundedBuffer(size_t bufferSize, TopLevelAllocatorBundle *tlaPushdown);
    ~PodBoundedBuffer();
    pod* getBufferedPod();
    pod *getBufferedPodCore();
    bool addPodToBuffer();
    int getBufferSize();
    bool boundedBufferIsFull();
    bool boundedBufferIsEmpty();

    void bufferThreadMain();
};


#endif //REMON_POD_BOUNDED_BUFFER_H
