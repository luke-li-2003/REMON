#include "SocketClient.h"

#include <utility>
#include <netdb.h>

SocketClient::SocketClient(std::string ipStr) {
    setLogPrefix("socket_client");
    this->ipStr = std::move(ipStr);
    std::stringstream ss;
    
    setRunning(false);
    while(!getRunning()) {
        if(initConnection() == 0) {
            setRunning(true);
            break;
        }
        sleep(1);
    }
}

int SocketClient::initConnection() {
    std::stringstream ss;

    if ((socketFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        err(ss << "Socket creation error");
        return -1;
    }
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(ipStr.c_str(), std::to_string(PORT).c_str(), &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    struct timeval tv{};
    tv.tv_sec = CLIENT_TIMEOUT;
    tv.tv_usec = 0;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    int optval = 1;


    if (setsockopt(socketFd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
        perror("setsockopt");
        close(socketFd);
        return 1;
    }
    
    if (connect(socketFd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(socketFd);
        freeaddrinfo(res);
        return -1;
    }
    freeaddrinfo(res);
    setRunning(true);
    return 0;
}

SocketClient::~SocketClient() {
    close(socketFd);
}

void SocketClient::reset() {
    std::stringstream ss;
    close(socketFd);
}