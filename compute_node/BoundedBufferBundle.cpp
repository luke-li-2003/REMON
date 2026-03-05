#include "BoundedBufferBundle.h"

BoundedBufferBundle::BoundedBufferBundle(size_t bufferNormalizedSize, TopLevelAllocatorBundle *tlaPushdown)
        : lock("bounded_buffer_bundle") {
    this->bufferNormalizedSize = bufferNormalizedSize;
    this->tlaPushdown = tlaPushdown;
}

BoundedBufferBundle::~BoundedBufferBundle() {
    for(auto i : podBoundedBufferMap) {
        delete i.second;
    }
}

PodBoundedBuffer *BoundedBufferBundle::getPodBoundedBuffer() {
    lock.lock(std::string());
    auto iter = podBoundedBufferMap.find(this_thread::get_id());
    if(iter == podBoundedBufferMap.end()) {
        auto insertedIter = podBoundedBufferMap.insert(make_pair(this_thread::get_id(), new PodBoundedBuffer(
                bufferNormalizedSize, tlaPushdown)));
        lock.unlock(std::string());
        return insertedIter.first->second;
    }
    lock.unlock(std::string());
    return iter->second;
}
