#ifndef REMON_REMOTE_CONNECTION_CLIENT_H
#define REMON_REMOTE_CONNECTION_CLIENT_H

#include "SocketClient.h"
#include "helperFunctions.h"

class pod;
#include "pod.h"
#include <cassert>

#define OPT_TCP_BUFF_SIZE M8

using namespace std;
class RemoteConnectionClient : public SocketClient {
private:
    int maxAvailablePods = -1;
    void getMaxAvailablePods();
public:
    RemoteConnectionClient(string ipStr);
    ~RemoteConnectionClient();
    bool savePodToRemote(pod* podPtr);
    bool highSpeedSendPod(pod *podPtr, size_t tailOffset);
    void getPodFromRemote(pod *podPtr, void *tmpPayload, bool isCache);
    void freePodInRemote(pod *pPod);
    void globalLock(string reason) {
#ifdef PRINT_LOCK
        stringstream ss;
        debug(ss << "rcc mux_lock: " << reason);
#endif
        lock.lock();
#ifdef PRINT_LOCK
        debug(ss << "rcc locked: " << reason);
#endif

    };

    void globalUnlock(string reason) {
#ifdef PRINT_LOCK
        stringstream ss;
        debug(ss << "rcc unlock: " << reason);
#endif
        lock.unlock();
#ifdef PRINT_LOCK
        debug(ss << "rcc unlocked: " << reason);
#endif
    }
    bool globalTryLock(string reason) {
#ifdef PRINT_LOCK
        stringstream ss;
        debug(ss << "rcc unlock: " << reason);
#endif
        return lock.try_lock();
#ifdef PRINT_LOCK
        debug(ss << "rcc unlocked: " << reason);
#endif
    }

    string initContext(size_t pageSize);

    void registerContext(const string& key);

    void memcmpDebug(char *A, char *B);
};


#endif //REMON_REMOTE_CONNECTION_CLIENT_H
