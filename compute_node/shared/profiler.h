#ifndef REMON_PROFILER_H
#define REMON_PROFILER_H

#include "string"
#include <iostream>
#include "timer.h"
#include "chrono"
#include "log.h"
using namespace std;

class profiler : public log{
    string name;
    size_t count = 0;
    size_t us = 0;
    
    std::chrono::time_point<std::chrono::steady_clock> from;
    std::chrono::time_point<std::chrono::steady_clock> to;
    mutex lock;
public:
    profiler(string name) : log("profiler"){
        this->name = name;
    }
    ~profiler(){
        stringstream ss;
        info(ss << "Profile: " << name << " count: " << count << " total us: " << us << " avg: " << us / (double)count); // << " worst: " << worst);
    }
    void record() {
        from = std::chrono::steady_clock::now();
    }
    void writeRecord() {
        to = std::chrono::steady_clock::now();
        size_t timeUsed = std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
        us += timeUsed;
        count ++;
    }
};


#endif //REMON_PROFILER_H
