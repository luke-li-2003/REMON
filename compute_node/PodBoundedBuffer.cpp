#include "PodBoundedBuffer.h"
#include "TopLevelAllocatorBundle.h"

PodBoundedBuffer::PodBoundedBuffer(size_t bufferSize, TopLevelAllocatorBundle *tlaPushdown)
        : log("pod_bounded_buffer"){
    buffer.resize(bufferSize);
    bufferLocks = new vector<mutex>(bufferSize);
    this->tlaPushdown = tlaPushdown;
    bufferKeeperThread = new thread(&PodBoundedBuffer::bufferThreadMain, this);
    while(!bufferThreadRunning);
}

pod *PodBoundedBuffer::getBufferedPodCore() {
    int partition = readIndex % buffer.size();
    unique_lock<mutex> lock((*bufferLocks)[partition]);
    while (boundedBufferIsEmpty()) {
        consumerCv.wait(lock);
    }
    pod *result = buffer[readIndex];
    readIndex = (readIndex + 1) % buffer.size();
    producerCv.notify_all();
    if(result) {
        result->lock("get_buffered_pod_core");
    }
    if (result->getState() == podState::podStateT::localPinned) {
        result->localPinnedToLocal();
    } else {
        stringstream ss;
        warn(ss << "get_buffered_pod_core return unexpected state: " << result->getState());
        result->unlock("did not get it");
        return nullptr;
    }
    return result;
}

bool PodBoundedBuffer::addPodToBuffer() {
    int partition = writeIndex % buffer.size();
    unique_lock<mutex> lock((*bufferLocks)[partition]);
    while (boundedBufferIsFull()) {
        producerCv.wait(lock);
        if (bufferThreadRunning == 0) {
            return false;
        }
    }
    pod *newPod = nullptr;
    while (true) {
        newPod = tlaPushdown->allocateNPages(1);
        if (newPod->tryLock("add_pod_to_buffer")) {
            break;
        }
    }
    newPod->headPod = (pod *) 0x000ADDB;
    auto podState = newPod->getState();
    switch (podState) {
        case podState::podStateT::init:
            newPod->initToLocalPinned();
            break;
        case podState::podStateT::local:
            newPod->localToLocalPinned();
            break;
        case podState::podStateT::disk:
            newPod->diskToInit();
            newPod->initToLocalPinned();
            break;
        case podState::podStateT::remote:
            newPod->remoteToInit();
            newPod->initToLocalPinned();
            break;
        case podState::podStateT::diskCache:
            newPod->diskCacheToLocalPinned();
            break;
        case podState::podStateT::remoteCache:
            newPod->remoteCacheToLocalPinned();
            break;
        default:
            stringstream ss;
            warn(ss << "unknown pod state in add_pod_to_buffer: " << newPod->getI() << " state: "
                    << podState);
    }
    newPod->unlock("add_pod_to_buffer");
    buffer[writeIndex] = newPod;
    writeIndex = (writeIndex + 1) % buffer.size();
    consumerCv.notify_all();
    return true;
}

int PodBoundedBuffer::getBufferSize() {
    return (writeIndex - readIndex + buffer.size()) % buffer.size();
}

bool PodBoundedBuffer::boundedBufferIsFull() {
    return ((writeIndex + 1) % buffer.size()) == readIndex;
}

bool PodBoundedBuffer::boundedBufferIsEmpty() {
    return readIndex == writeIndex;
}

void PodBoundedBuffer::bufferThreadMain() {
    bufferThreadRunning = 1;
    while (bufferThreadRunning) {
        if (!addPodToBuffer()) {
            bufferThreadRunning = -1;
            break;
        }
    }
}

PodBoundedBuffer::~PodBoundedBuffer() {
    bufferThreadRunning = 0;
    producerCv.notify_all();
    consumerCv.notify_all();
    bufferKeeperThread->join();
    delete bufferLocks;
}

pod *PodBoundedBuffer::getBufferedPod() {
    lock.lock();
    pod* pod = getBufferedPodCore();
    lock.unlock();
    return pod;
}
