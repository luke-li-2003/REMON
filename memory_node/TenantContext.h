#ifndef MEMORY_NODE_TENANT_CONTEXT_H
#define MEMORY_NODE_TENANT_CONTEXT_H
#include <string>
#include <unordered_map>

#include "log.h"
#include "tenantPod.h"
#include "RamWatch.h"

using namespace std;
class TenantContext : public log {
private:
    // pods slots
    // size_t vmSize = 0;
    // size_t podCount = 0;
    // void* base;
    // void initVm();
    // void initPods();
    // size_t pageSize;
public:
    explicit TenantContext(double ramSize);
    ~TenantContext();

    // vector<pod*> pods;
    unordered_map<string,tenantPod*> tenantPods;
    mutex tenantPodsMutex;
    RamWatch* ramWatcher;
    void lockTenantPodsLock() {
        tenantPodsMutex.lock();
    }
    void lockTenantPodsUnlock() {
        tenantPodsMutex.unlock();
    }
};


#endif //MEMORY_NODE_TENANT_CONTEXT_H
