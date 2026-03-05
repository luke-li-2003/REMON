#ifndef REMON_SOCKET_CLIENT_H
#define REMON_SOCKET_CLIENT_H

#include "communication.h"
#include <arpa/inet.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <netinet/in.h>
#include <netinet/tcp.h>

class SocketClient : public communication {
public:
    SocketClient(std::string ipStr);
    ~SocketClient();

    void reset();

    int initConnection();
};


#endif //REMON_SOCKET_CLIENT_H
