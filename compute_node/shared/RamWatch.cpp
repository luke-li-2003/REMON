#include "RamWatch.h"

RamWatch::RamWatch(double maxRam) : log("ram_watch") {
    this->maxRam = maxRam;
    stringstream ss;
    info(ss << "max ram: " << maxRam << " GB");
}

RamWatch::~RamWatch() {

}


void RamWatch::updateRamUsage() {
    lock.lock();
    ifstream statmStream("/proc/self/statm");
    size_t totalSizeInPages;
    size_t sizeInPages;

    statmStream >> totalSizeInPages;
    statmStream >> sizeInPages;
    statmStream.close();
    long pageSize = sysconf(_SC_PAGESIZE);
    ramUsageInGB = sizeInPages * pageSize * 1.0/ GB;
    
    lock.unlock();
}

bool RamWatch::ifNeedToSwap() {
    updateRamUsage();
    if(ramUsageInGB > maxRam) {
        return true;
    }
    return false;
}

double RamWatch::getRamUsage () {
    updateRamUsage();
    return ramUsageInGB;
}
