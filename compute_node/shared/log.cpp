#include "log.h"

std::mutex log::lock;
std::mutex log::lockf;
std::mutex log::lockstamp;

std::string log::getTimestamp() {
    auto currentTime = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(currentTime);
    std::tm timeinfo;
    localtime_r(&time, &timeinfo);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%a %b %d %H:%M:%S %Y", &timeinfo);
    std::string str(timestamp);
    return str.append(" " + getMiliStamp());
}

void log::info(std::ostream &ss) {
    shared(ss, "Info");
}

void log::warn(std::ostream &ss) {
    shared(ss, "Warn");
}

void log::err(std::ostream &ss) {
    shared(ss, "Error");
}

void log::debug(std::ostream &ss) {
    shared(ss, "Debug");
}

void log::shared(std::ostream &ss, const std::string& type) {
    return;
    /*
    if(!fileName.empty()) {
        return sharedf(ss, type);
    }
    */

    std::stringstream builder;
    if(prefix.size() < 64) {
        builder << type << " (" << prefix << ") (" << getThreadId() << ") : " << ss.rdbuf();
    } else {
        builder << type << " (" << "too long to show" << "): " << ss.rdbuf();
    }
    lock.lock();
    std::cout << builder.str() << std::endl;
    if(!fileName.empty()) {
        thisfd.open(fileName, std::ios::app);
        thisfd << builder.str() << std::endl;
        thisfd.close();
    }
    lock.unlock();
}

void log::sharedf(std::ostream &ss, const std::string& type) {
    std::ofstream fd;
    fd.open (fileName);
    if(fd.is_open()) {
        lockf.lock();
        std::cout << type << " (" << prefix << ") [" << getTimestamp() << "]: " << ss.rdbuf() << std::endl;
        lockf.unlock();
        fd.close();
    } else {
        failedOpenLogFile();
    }
}

void log::failedOpenLogFile() {
    lock.lock();
    std::cout << "Err (" << prefix << ") [" << getTimestamp() << "]: " << "Failed to open file: " << fileName << std::endl;
    lock.unlock();
}

std::string log::getMiliStamp() {
    return std::to_string(getMiliStampTimeT());
}

time_t log::getMiliStampTimeT() {
    struct timeval timeNow{};
    gettimeofday(&timeNow, nullptr);
    time_t msecsTime = (timeNow.tv_sec * 1000) + (timeNow.tv_usec / 1000);
    return msecsTime;
}

log::log(std::string prefix) {
    this->prefix = prefix;
}
