#include "SplMux.h"
#include "profiler.h"

SplMux::SplMux(size_t maxAllocationSize) : log("spl_mux"), muxLock("spl_mux") {
    this->maxAllocationSize = maxAllocationSize;
}

SplMux::~SplMux() {
    for(const auto& i : splMap) {
        delete i.second;
    }
}

SPLBundle * SplMux::splGet() {
    muxLock.lock(std::string());
    auto searchIt = splMap.find(this_thread::get_id());
    if (searchIt == splMap.end()) {
        SPLBundle* result = new SPLBundle(maxAllocationSize);
        splMap.insert(make_pair(this_thread::get_id(), result));
        muxLock.unlock(std::string());
        return result;
    } else {
        muxLock.unlock(std::string());
        return searchIt->second;
    }
}

void SplMux::trim() {
    muxLock.lock(std::string());
    for(auto pair : splMap) {
        pair.second->trim();
    }
    muxLock.unlock(std::string());
}
