#ifndef REMON_COMMUNICATION_H
#define REMON_COMMUNICATION_H

#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sstream>
#include "log.h"
#include "define.h"
#include <mutex>

class communication : public log {
private:
    volatile bool running;
protected:
    char buffer[REMON_BUFFER_SIZE] = {0};
    std::mutex lock;
public:
    std::string ipStr;
    int socketFd;
    struct sockaddr_in address;

    communication();

    ~communication();

    void bufferReset();

    int safeSend(int socket, const char localBuffer[], size_t sendSize);
    int safeSend(int, int);
    int safeSend();

    int safeRead(int socket, char localBuffer[], size_t readSize);
    int safeRead(int, int);
    int safeRead();
    
    bool getRunning() {
        lock.lock();
        volatile bool snapshot = running;
        lock.unlock();
        return snapshot;
    }

    void setRunning(bool value) {
        lock.lock();
        running = value;
        lock.unlock();
    }

    char *getBuffer() {
        return buffer;
    }

    size_t getBufferSize() {
        return REMON_BUFFER_SIZE;
    }

};



#endif //REMON_COMMUNICATION_H
