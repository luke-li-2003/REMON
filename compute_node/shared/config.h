#ifndef REMON_CONFIG_H
#define REMON_CONFIG_H

#include <string>
#include <fstream>
#include <vector>
#include "define.h"
#include "log.h"
#include <sstream>
#include <cstdlib>
#include "helperFunctions.h"
#include "mutex"
#include <filesystem>
#include "RemoteConnectionClient.h"
#include "unistd.h"
#include "libgen.h"
#include "sys/stat.h"

using namespace std;
class globalFunctions;
class config : public log {
private:
    string filePath;
    void generateConfigFile();

    mutex mtx;
public:
    double maxRamInGB = 0;
    double maxVmInGB = 0;
    size_t pageSize = DEFAULT_PAGE_SIZE;
    size_t topLevelArena = 1;
    vector<string> peers;
    string swapDirPath = "";
    vector<RemoteConnectionClient*>* remoteConnections;

    void clearSwapDir(pid_t pid, string processSwapDir);
    config();
    ~config();

    void clearSwapDirByPid(int pid);

    void setRemoteConnectionsPointer(vector<RemoteConnectionClient *> *pVector);

    void generateConfigFilePath();
    void generateStubSwapPath();

    void makeSwapDir(string dirname);
};


#endif //REMON_CONFIG_H
