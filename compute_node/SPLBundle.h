#ifndef REMON_SPL_BUNDLE_H
#define REMON_SPL_BUNDLE_H

#include "SegregatedPageList.h"
#include "log.h"
#include "SpinLock.h"

class SPLBundle : public log {
private:
    unordered_map<size_t, SegregatedPageList*> exactSizeMap;
    SpinLock lock;
public:
    SPLBundle(size_t maxAllocationSize);
    ~SPLBundle();

    SegregatedPageList *getSplBasedOnSize(size_t size);

    void trim();
};


#endif //REMON_SPL_BUNDLE_H
