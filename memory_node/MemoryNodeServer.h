#pragma once
#ifndef MEMORY_NODE_SERVER_H
#define MEMORY_NODE_SERVER_H

#include <thread>
#include <unordered_map>
#include <string>
#include <vector>
#include "SocketServer.h"
#include "mutex"
#include "config.h"
#include "TenantContext.h"
#include "tenantPod.h"

#include <csignal>

using namespace std;
class MemoryNodeServer : public SocketServer {
private:

    mutex contextMutex;
    unordered_map<int, TenantContext*> contextsByTid;
    unordered_map<int, int> workerToContextTid;

    // server controls
    thread* serverThread;
    volatile bool serverIsUp;
    void serviceMain();

    // function handlers
    int functionHandler(std::string str, int clientSocket, int tid, std::string ipStr) override;

    config globalConfig;




    void freeHook(int tid) override;
    void initHook(int tid) override;

    static void printBacktrace();

public:
    MemoryNodeServer();
    ~MemoryNodeServer();

    int getMaxPodsHandler(int clientSocket, int tid);
    int savePodHandler(const vector<string> &args, int clientSocket, const string &ipStr, int workerTid);
    int getPodHandler(vector<string> args, int socket, int workerTid, const string& ipStr);
    bool sendPodPayload(tenantPod *tenantPod, int clientSocket);
    int freePodHandler(vector<string> args, int clientSocket, const string& ipStr, int workerTid);

    void initContext(int tid);

    int contextInitHandler(const vector<string>& args, int clientSocket, int tid, const string &ipStr);

    int contextRegisterHandler(const vector<string> &args, int clientSocket, int tid, const string &ipStr);

    int contextTid(int workerTid);

    static void segfaultHandler(int signal, siginfo_t *si, void *arg);
    void segfaultRegistration();

    int getPodCacheHandler(vector<string> args, int socket, int workerTid, const string& ipStr);
};


#endif //MEMORY_NODE_SERVER_H
