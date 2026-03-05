#ifndef REMON_BOUNDED_BUFFER_BUNDLE_H
#define REMON_BOUNDED_BUFFER_BUNDLE_H

#include "log.h"
#include "PodBoundedBuffer.h"
#include "TopLevelAllocatorBundle.h"
#include "SpinLock.h"

class BoundedBufferBundle {
private:
    unordered_map<thread::id, PodBoundedBuffer*> podBoundedBufferMap;
    SpinLock lock;
    size_t bufferNormalizedSize;
    TopLevelAllocatorBundle * tlaPushdown = nullptr;
public:
    BoundedBufferBundle(size_t bufferNormalizedSize, TopLevelAllocatorBundle *tlaPushdown);
    ~BoundedBufferBundle();
    PodBoundedBuffer* getPodBoundedBuffer();
};


#endif //REMON_BOUNDED_BUFFER_BUNDLE_H
