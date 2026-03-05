#include "SPLBundle.h"

SPLBundle::SPLBundle(size_t maxAllocationSize) : log("SPL_bundle"), lock("SPL_bundle") {

}

SPLBundle::~SPLBundle() {

}

SegregatedPageList *SPLBundle::getSplBasedOnSize(size_t size) {
    lock.lock("get_spl_based_on_size");
    auto iter = exactSizeMap.find(size);
    if(iter == exactSizeMap.end()) {
        auto insertedIter = exactSizeMap.insert(make_pair(size, new SegregatedPageList));
        lock.unlock("get_spl_based_on_size");
        return insertedIter.first->second;
    }
    lock.unlock("get_spl_based_on_size");
    return iter->second;

}

void SPLBundle::trim() {
    lock.lock("trim");
    for(auto i : exactSizeMap) {
        i.second->trim();
    }
    lock.unlock("trim");
}
