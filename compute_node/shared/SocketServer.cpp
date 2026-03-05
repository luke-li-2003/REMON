#include "SocketServer.h"

SocketServer::SocketServer(){
    std::stringstream ss;
    setLogPrefix("socket_server");
}

int SocketServer::mainLoop() {
    std::stringstream ss;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    while (retry >= 0) {
        if (bind(socketFd, (struct sockaddr *) &address, sizeof(address)) < 0) {
            err(ss << "bind failed, retry (" << retry << ")");
            sleep(1);
        } else {
            info(ss << "bind OK");
            break;
        }
        if (retry == 0) {
            err(ss << "bind failed after retrying");
            return -1;
        }
        retry--;
    }
    if (listen(socketFd, 3) < 0) {
        err(ss << "listen failed");
        return -1;
    } else {
        info(ss << "listen OK");
    }

    setRunning(true);
    int addrlen = sizeof(struct sockaddr_in);

    info(ss << "entering server loop");
    while (getRunning()) {
        auto clientAddress = new sockaddr_in;
        if ((clientSocket = accept(socketFd, (struct sockaddr *) clientAddress, (socklen_t *) &addrlen)) < 0) {
            if (clientSocket == EINVAL) {
                err(ss << "socket_fd is closed");
                return 0;
            } else {
                err(ss << "accept failed");
                return -1;
            }
        }
        int optval = 1;
        if(setsockopt(clientSocket, SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval)) == -1) {
            perror("set socket opt");
            close(clientSocket);
            continue;
        }
        
        int tid = assignTid();
        auto currConnection = std::thread(&SocketServer::workerLoop, this, tid, (clientSocket), clientAddress);

        currConnection.detach();
    }
    
    close(socketFd);
    return 0;
}

int SocketServer::workerLoop(int tid, int clientSocketLocal, struct sockaddr_in *clientAddress) {
    std::stringstream ss;
    char localBuffer[REMON_BUFFER_SIZE] = {0};
    incrementActiveWorker();

    std::string peerIpStr = addrToStr(clientAddress);
    initHook(tid);
    int validCounter = 0;

    while (getRunning()) {
        int valread = recv(clientSocketLocal, localBuffer, REMON_BUFFER_SIZE, 0);
        if (valread == 0) {
            break;
        }
        if (valread < 0) {
            err(ss << "errno: " << strerror(errno));
            warn(ss << tid << ": [" << clientSocketLocal << "] client closed ungracefully @ " << peerIpStr << " history OK count: " << validCounter);
            break;
        }
        
        if (functionHandler(std::string(localBuffer), clientSocketLocal, tid, peerIpStr) == -1) {
            warn(ss << "tid: " << tid << ": server function_handler cannot handle cmd: " + std::string(localBuffer));
            std::string msg = std::to_string(remoteFunction::invalid);
            int sendErr = send(clientSocketLocal, msg.c_str(), strlen(msg.c_str()), MSG_NOSIGNAL);
            if (sendErr < 0) {
                warn(ss << "tid: " << tid << ": send back invalid cmd not working, client might be dead @ " << peerIpStr);
                break;
            }
        } else {
            validCounter++;
        }
        memset(localBuffer, 0, REMON_BUFFER_SIZE);
    }

    close(clientSocketLocal);

    freeHook(tid);
    delete clientAddress;

    decrementActiveWorker();
    info(ss << "worker " << tid << " exits");
    
    return 0;
}

SocketServer::~SocketServer() {
    std::stringstream ss;
    int index = 0;
    setRunning(false);
    info(ss << "server shutdown");
    sleep(1);
    
    close(socketFd);
}

std::string SocketServer::addrToStr(struct sockaddr_in *clientAddressCopy) {
    struct in_addr ipAddr = clientAddressCopy->sin_addr;
    char peerIpStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddr, peerIpStr, INET_ADDRSTRLEN);
    return peerIpStr;
}

int SocketServer::assignTid() {
    int result = -1;
    tidSlotLock.lock();
    if(tidSlots.empty()) {
        result = largestTid;
        largestTid++;
    } else {
        result = tidSlots.front();
        tidSlots.pop_front();
    }
    tidSlotLock.unlock();
    return result;
}

void SocketServer::returnTidToList(int tid) {
    tidSlotLock.lock();
    tidSlots.push_front(tid);
    tidSlotLock.unlock();
}
