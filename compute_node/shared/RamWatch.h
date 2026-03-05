#ifndef REMON_RAM_WATCH_H
#define REMON_RAM_WATCH_H

#include <fstream>
#include <unistd.h>
#include <mutex>
#include "log.h"

using namespace std;
class RamWatch : public log{
private:
    volatile double ramUsageInGB = 0;
    void updateRamUsage();
    double maxRam = 0;
    mutex lock;
public:
    RamWatch(double);
    ~RamWatch();
    bool ifNeedToSwap();

    double getRamUsage();
};


#endif //REMON_RAM_WATCH_H
