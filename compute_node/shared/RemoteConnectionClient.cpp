#include "RemoteConnectionClient.h"

RemoteConnectionClient::~RemoteConnectionClient() {

}

RemoteConnectionClient::RemoteConnectionClient(string ipStr) : SocketClient(ipStr) {
    stringstream ss;
    setLogPrefix("remote_connection_client");
    if(getRunning()) {
        info(ss << "Successfully connected to remote daemon @ " << ipStr);
    }else {
        err(ss << "should not arrive here");
    }
    
    int optval = OPT_TCP_BUFF_SIZE;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
    setsockopt(socketFd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
}

void RemoteConnectionClient::getMaxAvailablePods() {
    stringstream ss;
    remoteFunction::remoteFunctionT func = remoteFunction::getMaxPodsInit;
    stringstream bufferS;
    bufferS << func;

    bufferReset();
    strcpy(buffer, bufferS.str().c_str());
    safeSend();
    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=2) {
        err(ss << "Unexpected reply size for get_max_pods_init");
        return;
    }
    if(stoi(reply[0]) == remoteFunction::getMaxPodsOK) {
        maxAvailablePods = stoi(reply[1]);
        info(ss << "available ram size is " << reply[1] << " GB " << ipStr);
    }
}

bool RemoteConnectionClient::savePodToRemote(pod *podPtr) {
    remoteFunction::remoteFunctionT func = remoteFunction::savePodInit;
    
    size_t tailOffset = podPtr->getTailOffset();
    size_t sendSize = tailOffset;
    stringstream bufferS;
    bufferS << func << " " << podPtr->getI() << " " << sendSize;
    bufferReset();
    strcpy(buffer, bufferS.str().c_str());
    safeSend();
    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=1) {
        stringstream ss;
        err(ss << "Unexpected reply size for save_pod_to_remote: " << buffer);
        return false;
    }
    if(stoi(reply[0]) == remoteFunction::savePodOK) {
        podPtr->setState(podState::podStateT::remote);
        if(!podPtr->toReadOnly()) {
            podPtr->toReadAndWrite();
            stringstream ss;
            err(ss << "!pod_ptr->to_read_only(): " << podPtr->getI());
            return false;
        }
        if(!highSpeedSendPod(podPtr, tailOffset)) {
            stringstream ss;
            err(ss << "failed during sending the pod " << podPtr->getI());
            podPtr->toReadAndWrite();
            return false;
        }
        podPtr->toNone();
    } else if (stoi(reply[0]) == remoteFunction::savePodErr)  {
        stringstream ss;
        warn(ss << "save content of the pod to remote rejected");
        return false;
    }
    return true;
}

bool RemoteConnectionClient::highSpeedSendPod(pod *podPtr, size_t tailOffset) {
    
    auto curr = reinterpret_cast<uint8_t*>(podPtr->getPayload());
    auto end = reinterpret_cast<uint8_t*>(curr) + tailOffset;
    while(curr < end) {
        size_t sentBytes;
        size_t leftToSend = end - curr;
        if(leftToSend >= IO_RATE) {
            sentBytes = safeSend(socketFd, reinterpret_cast<const char *>(curr), IO_RATE);
        } else {
            sentBytes = safeSend(socketFd, reinterpret_cast<const char *>(curr), leftToSend);
        }
        if (sentBytes == -1) {
            return false;
        }
        curr += sentBytes;
    }
    assert(curr == end);
    
    if(tailOffset != podPtr->getSize()) {
        podPtr->debugTail = new char[QSIZE];
        memcpy(podPtr->debugTail, reinterpret_cast<char *>(podPtr->getPayload()) + tailOffset, DSIZE);
        memcpy(podPtr->debugTail+DSIZE, reinterpret_cast<char *>(podPtr->getPayload()) + podPtr->getSize() - DSIZE, DSIZE);
    }
    podPtr->sentOffset = tailOffset;
    
    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=1) {
        stringstream ss;
        err(ss << "Unexpected reply size for high_speed_send_pod");
        return false;
    } else if (stoi(reply[0]) == remoteFunction::savePodErr)  {
        stringstream ss;
        warn(ss << "save content of the pod to remote failed");
        return false;
    } else if (stoi(reply[0]) == remoteFunction::savePodDONE)  {
        return true;
    }
    return true;
}

void RemoteConnectionClient::getPodFromRemote(pod *podPtr, void *tmpPayload, bool isCache) {
    remoteFunction::remoteFunctionT func;
    if(isCache) {
        func = remoteFunction::getPodCacheInit;
    } else {
        func = remoteFunction::getPodInit;
    }
    stringstream bufferS;
    bufferS << func << " " << podPtr->getI();
    safeSend(socketFd, bufferS.str().c_str(), bufferS.str().length());

    char* curr = reinterpret_cast<char*>(tmpPayload);
    char* end = curr + podPtr->sentOffset;
    while(curr < end) {
        size_t leftToRead = end - curr;
        int readSize;
        if(leftToRead >= IO_RATE) {
            readSize = safeRead(socketFd, curr, IO_RATE);
        } else {
            readSize = safeRead(socketFd, curr, leftToRead);
        }
        if(readSize == -1) {
            return;
        }
        curr += readSize;
    }
    assert(curr == end);

    if(podPtr->sentOffset != podPtr->getSize()) {
        memcpy(reinterpret_cast<char*>(tmpPayload) + podPtr->sentOffset, podPtr->debugTail, DSIZE);
        char* tailAddr = reinterpret_cast<char*>(tmpPayload) + podPtr->getSize() - DSIZE;
        memcpy(tailAddr, podPtr->debugTail + DSIZE, DSIZE);
        if(!isCache) {
            delete[] podPtr->debugTail;
        }
    }
}

void RemoteConnectionClient::freePodInRemote(pod *podPtr) {
    stringstream ss;
    
    remoteFunction::remoteFunctionT func = remoteFunction::freePodInit;
    stringstream bufferS;
    bufferS << func << " " << podPtr->getI();
    bufferReset();
    strcpy(buffer, bufferS.str().c_str());
    safeSend();
    delete[] podPtr->debugTail;

    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=1) {
        err(ss << "free_pod_in_remote wrong # of reply");
        return;
    } else {
        if(stoi(reply[0]) != remoteFunction::freePodOk) {
            err(ss << "free_pod_in_remote failed");
            return;
        } else {
            return;
        }
    }
}

string RemoteConnectionClient::initContext(size_t pageSize) {
    remoteFunction::remoteFunctionT func = remoteFunction::contextInit;
    stringstream bufferS;
    bufferS << func;

    safeSend(socketFd, bufferS.str().c_str(), bufferS.str().length());
    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=2) {
        stringstream ss;
        err(ss << "Unexpected reply size for init_context");
        return "";
    } else if (stoi(reply[0]) == remoteFunction::contextInitOk) {
        stringstream ss;
        info(ss << "context_init_ok");
        return reply[1];
    }
    return "";
}

void RemoteConnectionClient::registerContext(const string& key) {
    remoteFunction::remoteFunctionT func = remoteFunction::contextRegister;
    stringstream bufferS;
    bufferS << func << " " << key;
    
    safeSend(socketFd, bufferS.str().c_str(), bufferS.str().length());
    bufferReset();
    safeRead();
    vector<string> reply = tokenizeString(buffer);
    if(reply.size()!=1) {
        stringstream ss;
        err(ss << "Unexpected reply size for register_context: " << buffer);
        return;
    } else if (stoi(reply[0]) == remoteFunction::contextRegisterOk) {
        stringstream ss;
        return;
    }
}

void RemoteConnectionClient::memcmpDebug(char *A, char *B) {
    std::cout << "Memory contents of str1: ";
    for (size_t i = 0; i < sizeof(A); ++i) {
        std::cout << std::hex << (int)A[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "Memory contents of str2: ";
    for (size_t i = 0; i < sizeof(B); ++i) {
        std::cout << std::hex << (int)B[i] << " ";
    }
    std::cout << std::endl;
}
