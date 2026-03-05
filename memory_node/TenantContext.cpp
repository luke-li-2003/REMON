#include "TenantContext.h"

TenantContext::TenantContext(double ramSize) : log("thread_context") {
    stringstream ss;
    info(ss << "create context");
    ramWatcher = new RamWatch(ramSize);
}

TenantContext::~TenantContext() {
    stringstream ss;
    info(ss << "delete context");
    for(const auto& i : tenantPods) {
        delete i.second;
    }
    delete ramWatcher;
    info(ss << "delete context done");
}
