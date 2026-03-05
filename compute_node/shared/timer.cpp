#include "timer.h"

timer::timer(std::string name) : log("timer") {
    name = std::move(name);
    from = std::chrono::steady_clock::now();
}

void timer::reset() {
    from = std::chrono::steady_clock::now();
}

void timer::report() {
    to = std::chrono::steady_clock::now();
    int ms = std::chrono::duration_cast<std::chrono::milliseconds>(to - from).count();
    std::stringstream ss;
    info(ss << name << " takes: " << ms << " ms");
}

void timer::reportAccurate() {
    to = std::chrono::steady_clock::now();
    int us = std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
    std::stringstream ss;
    info(ss << name << " takes: " << us << " us");
}

size_t timer::getUs() {
    to = std::chrono::steady_clock::now();
    int us = std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
    return us;
}