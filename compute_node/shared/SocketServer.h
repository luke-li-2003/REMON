#ifndef REMON_SOCKET_SERVER_H
#define REMON_SOCKET_SERVER_H

#include "communication.h"
#include "define.h"
#include <vector>
#include <thread>
#include <mutex>
#include <arpa/inet.h>
#include <unordered_map>
#include "mutex"
#include <list>

using namespace std;

class SocketServer : public communication {
private:
    int retry = MAX_BIND_RETRY;
    int clientSocket;
    int activeWorker = 0;
    std::mutex lock;
    std::vector<std::thread *> connections;
    list<int> tidSlots;
    int largestTid = 0;
    mutex tidSlotLock;

public:
    SocketServer();
    ~SocketServer();

    int getActiveWorker() {
        return activeWorker;
    }

    void incrementActiveWorker() {
        lock.lock();
        activeWorker++;
        lock.unlock();
    }

    void decrementActiveWorker() {
        lock.lock();
        activeWorker--;
        lock.unlock();
    }

    int mainLoop();

    int workerLoop(int tid, int clientSocket, struct sockaddr_in *);

    virtual void initHook(int tid) = 0;
    virtual void freeHook(int tid) = 0;

    std::string addrToStr(struct sockaddr_in *);

    virtual int functionHandler(std::string str, int clientSocket, int tid, std::string ipStr) {
        std::stringstream ss;
        warn(ss << "should not call the base version");
        return 0;
    };

    int assignTid();

    void returnTidToList(int tid);
};


#endif //REMON_SOCKET_SERVER_H
