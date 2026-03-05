#ifndef REMON_REMOTE_CONNECTION_BUNDLE_H
#define REMON_REMOTE_CONNECTION_BUNDLE_H

#include "log.h"
#include "pod.h"
#include "RemoteConnectionClient.h"
#include "SpinLock.h"
#include <unordered_map>

class RemoteConnectionClient;
class RemoteConnectionBundle : public log {
    
private:
    mutex globalMutex;
    RemoteConnectionClient* mainClientConnection;
    mutex clientConnectionsMapMutex;
    unordered_map<thread::id, RemoteConnectionClient*> clientConnectionsMap;
    vector<RemoteConnectionClient*> arrayClientConnections;
    mutex remoteSessionKeyLock;
    string remoteSessionKey;

    size_t getRemoteSessionKeyLength() {
        int length = remoteSessionKey.length();
        return length;
    }
    void globalLock(string reason) {
        globalMutex.lock();
    };
    void globalUnlock(string reason) {
        globalMutex.unlock();
    };
    size_t pageSize;

public:
    string ipStr;
    explicit RemoteConnectionBundle(const std::basic_string<char> &ipStr, size_t pageSize);
    ~RemoteConnectionBundle();

    bool savePodToRemote(pod *podOfInterest);

    void getPodFromRemote(pod *pPod, void *pVoid);

    void contextInitCheck();

    void registerWorkingSockets();

    RemoteConnectionClient *getAUniqueConnectionForTheThread();

    void updateRemoteSessionKey(string key);

    const string &getRemoteSessionKey();

    void freeRemotePod(pod *podOfInterest);

    void getPodCacheFromRemote(pod *pPod, void *pVoid);
};


#endif //REMON_REMOTE_CONNECTION_BUNDLE_H
