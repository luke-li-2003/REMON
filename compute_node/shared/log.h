#ifndef REMON_LOG_H
#define REMON_LOG_H

#include <string>
#include <iostream>
#include <ctime>
#include <cstring>
#include <sstream>
#include <mutex>
#include <cerrno>
#include <fstream>
#include <sys/time.h>
#include <thread>
#include <sstream>

#include "define.h"

class log {

private:
    void shared(std::ostream &, const std::string&);
    void sharedf(std::ostream &ss, const std::string&);
    std::string prefix;

public:
    log(std::string prefix);

    static std::mutex lock;
    static std::mutex lockf;
    static std::mutex lockstamp;

    void setLogPrefix(std::string prefix){
        this->prefix = prefix;
        if(prefix.size() > 64) {
            exit(-1);
        }
    }

    std::string fileName = "";//"./remon.log";
    std::ofstream thisfd;

    std::string getTimestamp();
    std::string getMiliStamp();

    void info(std::ostream &);

    void warn(std::ostream &);

    void err(std::ostream &);

    void debug(std::ostream &);


    void failedOpenLogFile();

    time_t getMiliStampTimeT();

    std::string getThreadId() {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        return ss.str();
    }
};

#endif //REMON_LOG_H
