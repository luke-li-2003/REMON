#include "RemoteConnectionBundle.h"

#include <utility>

RemoteConnectionBundle::RemoteConnectionBundle(const std::basic_string<char> &ipStr, size_t pageSize)
        : log("remote_connection_bundle"){
    mainClientConnection = new RemoteConnectionClient(ipStr);
    
    this->ipStr = ipStr;
    this->pageSize = pageSize;
}

RemoteConnectionBundle::~RemoteConnectionBundle() {
    for(auto & clientConnection : clientConnectionsMap) {
        delete clientConnection.second;
    }
    delete mainClientConnection;
}

bool RemoteConnectionBundle::savePodToRemote(pod *podOfInterest) {
    stringstream ss;
    contextInitCheck();
    RemoteConnectionClient* lockedClient = getAUniqueConnectionForTheThread();
    
    if(lockedClient->savePodToRemote(podOfInterest)) {
        podOfInterest->localToRemote(this); 
        return true;
    }

    warn(ss << "swap to remote failed: " << ipStr);
    return false;
}

void RemoteConnectionBundle::getPodFromRemote(pod *podOfInterest, void *memDest) {
    RemoteConnectionClient* lockedClient = getAUniqueConnectionForTheThread();
    lockedClient->getPodFromRemote(podOfInterest, memDest, false);
}

void RemoteConnectionBundle::contextInitCheck() {
    stringstream ss;
    if(getRemoteSessionKeyLength() == 0) {
        globalLock("init context");
        if(getRemoteSessionKeyLength()) {
            globalUnlock("inited context");
            return;
        }
        info(ss << "context_init_check");
        string result = mainClientConnection->initContext(pageSize);
        if(result.length() == 0) {
            err(ss << "context_init failed");
        }
        info(ss << "remote_session_key: " << remoteSessionKey);
        updateRemoteSessionKey(result);
        globalUnlock("inited context");
    }
}

void RemoteConnectionBundle::registerWorkingSockets() {
    /*for(auto & client_connection : client_connections_map) {
        client_connection->register_context(remote_session_key);
    }*/
}

RemoteConnectionClient *RemoteConnectionBundle::getAUniqueConnectionForTheThread() {
    clientConnectionsMapMutex.lock();
    auto searchIt = clientConnectionsMap.find(this_thread::get_id());
    if (searchIt == clientConnectionsMap.end()) {
        auto* newConnection = new RemoteConnectionClient(ipStr);
        clientConnectionsMap.insert(make_pair(this_thread::get_id(), newConnection));
        clientConnectionsMapMutex.unlock();
        newConnection->registerContext(getRemoteSessionKey());
        return newConnection;
    } else {
        clientConnectionsMapMutex.unlock();
        return searchIt->second;
    }
}

void RemoteConnectionBundle::updateRemoteSessionKey(string key) {
    remoteSessionKey = std::move(key);
}

const string &RemoteConnectionBundle::getRemoteSessionKey() {
    return remoteSessionKey;
}

void RemoteConnectionBundle::freeRemotePod(pod *podOfInterest) {
    RemoteConnectionClient* lockedClient = getAUniqueConnectionForTheThread();
    lockedClient->freePodInRemote(podOfInterest);
}

void RemoteConnectionBundle::getPodCacheFromRemote(pod *podOfInterest, void *memDest) {
    RemoteConnectionClient* lockedClient = getAUniqueConnectionForTheThread();
    lockedClient->globalLock("get_pod_cache_from_remote");
    lockedClient->getPodFromRemote(podOfInterest, memDest, true);
    lockedClient->globalUnlock("get_pod_cache_from_remote");
}