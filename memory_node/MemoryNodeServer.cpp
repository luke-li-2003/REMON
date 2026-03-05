#include "MemoryNodeServer.h"
#include "helperFunctions.h"
#include <execinfo.h>
#include <cxxabi.h>

MemoryNodeServer::MemoryNodeServer() {
    stringstream ss;
    serverIsUp = false;
    serverThread = new std::thread(&MemoryNodeServer::serviceMain, this);
    while(!serverIsUp);
    info(ss << "interface_server: created server main thread");
    while(!getRunning());
    info(ss << "interface_server: socket server is client_worker_running");
    setLogPrefix("interface_server");
}

MemoryNodeServer::~MemoryNodeServer() {
    stringstream ss;
    info(ss << "interface_server shutdown");
}

void MemoryNodeServer::serviceMain() {
    serverIsUp = true;
    mainLoop();
}

int MemoryNodeServer::functionHandler(std::string str, int clientSocket, int tid, std::string ipStr) {

    std::string reply;
    if (str.empty()) {
        std::stringstream ss;
        err(ss << "empty reply received");
        return -1;
    }
    
    std::vector<std::string> strVec = tokenizeString(str);
    int switchType = stoi(strVec[0]);
    switch (switchType) {
        case remoteFunction::getMaxPodsInit:
            return getMaxPodsHandler(clientSocket, tid);
        case remoteFunction::savePodInit:
            return savePodHandler(strVec, clientSocket, ipStr, tid);
        case remoteFunction::getPodInit:
            return getPodHandler(strVec, clientSocket, tid, ipStr);
        case remoteFunction::getPodCacheInit:
            return getPodCacheHandler(strVec, clientSocket, tid, ipStr);
        case remoteFunction::freePodInit:
            return freePodHandler(strVec, clientSocket, ipStr, tid);
        case remoteFunction::contextInit:
            return contextInitHandler(strVec, clientSocket, tid, ipStr);
        case remoteFunction::contextRegister:
            return contextRegisterHandler(strVec, clientSocket, tid, ipStr);
        default:
            std::stringstream ss;
            warn(ss << "unhandled case: " << switchType << " raw_str: " << str);
    }
    return 0;
}

int MemoryNodeServer::getMaxPodsHandler(int clientSocket, int tid) {
    remoteFunction::remoteFunctionT func = remoteFunction::getMaxPodsOK;
    stringstream bufferS;
    bufferS << func << " " << globalConfig.maxRamInGB;
    safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    return 0;
}

int MemoryNodeServer::savePodHandler(const vector<string> &args, int clientSocket, const string &ipStr, int workerTid) {
    int tid = contextTid(workerTid);
    contextMutex.lock();
    TenantContext* context = contextsByTid.find(tid)->second;
    if(context == nullptr) {
        contextMutex.unlock();
        return -1;
    }
    contextMutex.unlock();

    if(context->ramWatcher->ifNeedToSwap()) {
        remoteFunction::remoteFunctionT func = remoteFunction::savePodErr;
        stringstream bufferS;
        bufferS << func;
        safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
        return 0;
    }
    if(args.size() != 3) {
        stringstream ss;
        err(ss << "expecting func, str, size total 3 args");
        return -1;
    }
    
    size_t flexPageSize = stol(args[2]);
    auto* currPod = new tenantPod(flexPageSize);
    context->lockTenantPodsLock();
    context->tenantPods.insert(make_pair<>(args[1], currPod));
    context->lockTenantPodsUnlock();

    remoteFunction::remoteFunctionT func = remoteFunction::savePodOK;
    stringstream bufferS;
    bufferS << func;
    safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    char* curr = (char*) currPod->getPayload();
    char* end = curr + flexPageSize;

    while(curr < end) {
        size_t leftToRead = end - curr;
        int readSize;
        if(leftToRead >= IO_RATE) {
            readSize = safeRead(clientSocket, curr, IO_RATE);
        } else {
            readSize = safeRead(clientSocket, curr, leftToRead);
        }
        if(readSize == -1) {
            stringstream ss;
            err(ss << "read failed");
            return -1;
        }
        curr += readSize;
    }

    if(curr != end) {
        stringstream ss;
        err(ss << "curr != end");
        return -1;
    }

    func = remoteFunction::savePodDONE;
    bufferS.str("");
    bufferS << func;
    int ret = safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    if(ret < 0) {
        stringstream ss;
        err(ss << "send DONE failed");
    }

    return 0;
}

int MemoryNodeServer::getPodHandler(vector<string> args, int clientSocket, int workerTid, const string& ipStr) {
    int tid = contextTid(workerTid);
    contextMutex.lock();
    TenantContext* context = contextsByTid.find(tid)->second;
    if(context == nullptr) {
        contextMutex.unlock();
        return -1;
    }
    contextMutex.unlock();
    context->lockTenantPodsLock();
    tenantPod* POI = nullptr;
    auto searchIt = context->tenantPods.find(args[1]);
    if (searchIt != context->tenantPods.end()) {
        POI = (*searchIt).second;
    }

    if(POI == nullptr) {
        context->lockTenantPodsUnlock();
        stringstream ss;
        warn(ss << "getPodHandler: pod not found for key: " << args[1]);
        remoteFunction::remoteFunctionT func = remoteFunction::getPodErr;
        stringstream bufferS;
        bufferS << func;
        safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
        return 0;
    }

    context->lockTenantPodsUnlock();

    // client is ready
    if(sendPodPayload(POI, clientSocket)) {
        context->lockTenantPodsLock();
        delete POI;
        context->tenantPods.erase(searchIt);
        context->lockTenantPodsUnlock();
        return 0;
    }

    return 0;
}

bool MemoryNodeServer::sendPodPayload(tenantPod *tenantPod, int clientSocket) {
    if(tenantPod == nullptr) {
        stringstream ss;
        err(ss << "sendPodPayload: null tenantPod");
        return false;
    }
    auto curr = reinterpret_cast<uint8_t*>(tenantPod->getPayload());
    auto end = reinterpret_cast<uint8_t*>(curr) + tenantPod->getPageSize();
    while(curr < end) {
        size_t sentBytes;
        size_t leftToSend = end - curr;
        if(leftToSend >= IO_RATE) {
            sentBytes = safeSend(clientSocket, reinterpret_cast<const char *>(curr), IO_RATE);
        } else {
            sentBytes = safeSend(clientSocket, reinterpret_cast<const char *>(curr), leftToSend);
        }
        if(sentBytes == -1) {
            stringstream ss;
            err(ss << "send failed");
            return false;
        }
        curr += sentBytes;
    }
    return true;
}

int MemoryNodeServer::freePodHandler(vector<string> args, int clientSocket, const string& ipStr, int workerTid) {
    int tid = contextTid(workerTid);
    contextMutex.lock();
    TenantContext* context = contextsByTid.find(tid)->second;
    if(context == nullptr) {
        contextMutex.unlock();
        return -1;
    }
    contextMutex.unlock();
    context->lockTenantPodsLock();
    auto searchIt = context->tenantPods.find(args[1]);
    if (searchIt != context->tenantPods.end()) {
        delete (*searchIt).second;
        context->tenantPods.erase(searchIt);
    }
    context->lockTenantPodsUnlock();
    remoteFunction::remoteFunctionT func = remoteFunction::freePodOk;
    stringstream bufferS;
    bufferS << func;
    safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    return 0;

}

void MemoryNodeServer::freeHook(int tid) {
    contextMutex.lock();
    auto searchIt = contextsByTid.find(tid);
    if (searchIt != contextsByTid.end()) {
        delete (*searchIt).second;
        contextsByTid.erase(searchIt);
    }
    auto searchWorkerIt = workerToContextTid.find(tid);
    if (searchWorkerIt != workerToContextTid.end()) {
        workerToContextTid.erase(searchWorkerIt);
    }
    returnTidToList(tid);
    contextMutex.unlock();
}

void MemoryNodeServer::initHook(int tid) {
    // init_context(tid);
}

void MemoryNodeServer::initContext(int tid) {
    stringstream ss;
    info(ss << "init context for tid: " << tid);
    contextMutex.lock();
    contextsByTid.insert(make_pair(tid, new TenantContext(globalConfig.maxRamInGB)));
    contextMutex.unlock();
    info(ss << "inited context for tid: " << tid);
}

int MemoryNodeServer::contextInitHandler(const vector<string>& args, int clientSocket, int tid, const string& ipStr) {
    if(args.size() != 1) {
        stringstream ss;
        err(ss << "1 args is neede for context_init_handler");
        return -1;
    }
    initContext(tid);
    remoteFunction::remoteFunctionT func = remoteFunction::contextInitOk;
    stringstream bufferS;
    bufferS << func << " " << tid;
    safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    return 0;
}

int MemoryNodeServer::contextRegisterHandler(const vector<string>& args, int clientSocket, int tid, const string& ipStr) {
    stringstream ss;
    if(args.size() != 2) {
        err(ss << "context_register_handler: unexpected # of args: " << args.size());
        return -1;
    }
    int mapTo = stoi(args[1]);
    contextMutex.lock();
    workerToContextTid.insert(make_pair(tid, mapTo));
    contextMutex.unlock();
    debug(ss << "tid: " << tid << " map to: " << mapTo);
    remoteFunction::remoteFunctionT func = remoteFunction::contextRegisterOk;
    stringstream bufferS;
    bufferS << func;
    safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
    return 0;
}

int MemoryNodeServer::contextTid(int workerTid) {
    contextMutex.lock();
    auto searchIt = workerToContextTid.find(workerTid);
    if (searchIt == workerToContextTid.end()) {
        stringstream ss;
        err(ss << "cannot find mapping for tid: " << workerTid);
    } else {
        int mappedTid = searchIt->second;
        contextMutex.unlock();
        return mappedTid;
    }
    contextMutex.unlock();
    return -1;
}

void MemoryNodeServer::printBacktrace() {
    cout << "print backtrace" << endl;
    const int maxTraceSize = 20;
    void* stackTrace[maxTraceSize];
    int stackSize = backtrace(stackTrace,maxTraceSize);
    char** symbols = backtrace_symbols(stackTrace, stackSize);
    if(symbols == nullptr) {
        cout << "cannot dump symbols" << endl;
    }
    for(int i = 0; i < stackSize; i++) {
        int status;
        char* demangledSymbol = abi::__cxa_demangle(symbols[i], nullptr, nullptr, &status);
        if(demangledSymbol!= nullptr) {
            cout << "D:" << demangledSymbol<< endl;
            free(demangledSymbol);
        } else {
            cout << symbols[i]<< endl;
        }
    }
    free(symbols);
    exit(-1);
}

void MemoryNodeServer::segfaultRegistration() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = MemoryNodeServer::segfaultHandler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);
}

void MemoryNodeServer::segfaultHandler(int signal, siginfo_t *si, void *arg) {
    printBacktrace();
}

int MemoryNodeServer::getPodCacheHandler(vector<string> args, int clientSocket, int workerTid, const string& ipStr) {
    int tid = contextTid(workerTid);
    contextMutex.lock();
    TenantContext* context = contextsByTid.find(tid)->second;
    if(context == nullptr) {
        contextMutex.unlock();
        return -1;
    }
    contextMutex.unlock();
    
    context->lockTenantPodsLock();
    tenantPod* POI = nullptr;
    auto searchIt = context->tenantPods.find(args[1]);
    if (searchIt != context->tenantPods.end()) {
        POI = (*searchIt).second;
    }
    context->lockTenantPodsUnlock();

    if(POI == nullptr) {
        stringstream ss;
        warn(ss << "getPodCacheHandler: pod not found for key: " << args[1]);
        remoteFunction::remoteFunctionT func = remoteFunction::getPodCacheErr;
        stringstream bufferS;
        bufferS << func;
        safeSend(clientSocket, bufferS.str().c_str(), bufferS.str().length());
        return 0;
    }

    // client is ready
    sendPodPayload(POI, clientSocket);

    return 0; 
}
