#ifndef REMON_TIMER_H
#define REMON_TIMER_H

#include <chrono>
#include <string>
#include "log.h"

class timer : public log {
private:
    std::string name;
    std::chrono::time_point<std::chrono::steady_clock> from;
    std::chrono::time_point<std::chrono::steady_clock> to;
public:
    timer(std::string name);

    void report();
    void reset();
    void reportAccurate();
    size_t getUs();
};



#endif // REMON_TIMER_H
