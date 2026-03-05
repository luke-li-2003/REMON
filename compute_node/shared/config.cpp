#include "config.h"

config::config() : log("config") {
    stringstream ss;
    generateConfigFilePath();
    ifstream fh(filePath);
    if(!fh.is_open()) {
        info(ss << "Generating default config file");
        
        maxRamInGB = MAX_RAM_DEFAULT;
        maxVmInGB = MAX_VM_DEFAULT;
        generateConfigFile();
        return;
    }
    topLevelArena = sysconf(_SC_NPROCESSORS_ONLN) * 2; 
    info(ss << "Loading config file");
    string line;
    while (getline (fh, line)) {
        vector<string> cols = tokenizeString(line);
        if(cols.empty()) {
            continue;
        }
        if(cols[0].rfind("#", 0) == 0) {
            continue;
        }
        if(cols[0] == "MAX_RAM_IN_GB:") {
            maxRamInGB = stod(cols[1]);
            info(ss << "Loaded MAX_RAM_IN_GB: " << maxRamInGB);
        } else if(cols[0] == "MAX_VM_IN_GB:") {
            maxVmInGB = stod(cols[1]);
            info(ss << "Loaded MAX_VM_IN_GB: " << maxVmInGB);
        } else if(cols[0] == "PEER:") {
            for (int i = 1; i < cols.size(); i++) {
                peers.push_back(cols[i]);
                info(ss << "Loaded peer: " << cols[i]);
            }
            if(peers.empty()) {
                info(ss << "There is no peer");
            }
        } else if(cols[0] == "SWAP_DISK:" and cols.size() == 2) {
            swapDirPath = cols[1];
            info(ss << "Loaded swap disk: " << cols[1]);

        } else if(cols[0] == "PAGE_SIZE:" and cols.size() == 2) {
            stringstream sstream(cols[1]);
            size_t proposedPageSize;
            sstream >> proposedPageSize;
            if(proposedPageSize % K4 != 0) {
                warn(ss << "rejecting page size: " << proposedPageSize << " as it is not divisible by: " << IO_RATE);
            } else {
                pageSize = proposedPageSize;
            }
            info(ss << "PAGE_SIZE: " << pageSize);
        } else if(cols[0] == "TOP_LEVEL_ARENA:" and cols.size() == 2) {
            stringstream sstream(cols[1]);
            sstream >> topLevelArena;
            info(ss << "TOP_LEVEL_ARENA: " << topLevelArena);
        }
    }
    fh.close();
}

config::~config() {

}

void config::generateConfigFile() {
    stringstream ss;
    info(ss << "generating config file at: " << filePath);
    ofstream fh(filePath);

    if(!fh.is_open()) {
        err(ss << "cannot generate config file");
        return;
    }
    fh << "MAX_RAM_IN_GB: " << maxRamInGB << endl;
    fh << "MAX_VM_IN_GB: " << maxVmInGB << endl;
    fh << "PEER: ";
    for(const auto& peer : peers) {
        fh << peer << " ";
    }
    fh << endl;
    generateStubSwapPath();
    fh << "SWAP_DISK: " << swapDirPath << endl;
    fh << "PAGE_SIZE: " << DEFAULT_PAGE_SIZE << endl;
    fh.close();
}

void config::clearSwapDir(pid_t pid, string processSwapDir) {
    stringstream ss;
    info(ss << "clean swap path: " << processSwapDir);
    if (processSwapDir.empty()) {
        warn(ss << "clearSwapDir called with empty path");
        return;
    }
    if (!filesystem::exists(processSwapDir)) {
        return;
    }
    for (const auto & entry : filesystem::directory_iterator(processSwapDir)) {
        filesystem::remove_all(entry.path());
    }
}

void config::clearSwapDirByPid(int pid) {
    string path = swapDirPath + "/" + to_string(pid);
    try
    {
        std::filesystem::remove_all(path);
    }
    catch (std::filesystem::filesystem_error &e) {
        cout << e.what() << endl;
    }
}

void config::setRemoteConnectionsPointer(vector<RemoteConnectionClient *> *pushdownPtr) {
    remoteConnections = pushdownPtr;
}

void config::generateConfigFilePath() {
    stringstream ss;
    char buffer[REMON_BUFFER_SIZE];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if(len != -1) {
        buffer[len] = '\0';
        string dir = dirname(buffer);
        filePath = dir + "/remon.config";
        info(ss << "config file path: " << filePath);
    } else {
        err(ss << "Error: cannot generate swap file path");
    }
}

void config::generateStubSwapPath() {
    stringstream ss;
    char buffer[REMON_BUFFER_SIZE];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if(len != -1) {
        buffer[len] = '\0';
        string dir = dirname(buffer);
        swapDirPath = dir + "/remon_swap";
        info(ss << "swap dir: " << swapDirPath);
        makeSwapDir(swapDirPath);
    } else {
        err(ss << "Error: cannot generate swap file path");
    }
}

void config::makeSwapDir(string dirname) {
    stringstream ss;
    int ret = mkdir(dirname.c_str(), 0700);
    if(ret) {
        err(ss << "cannot mkdir: " << dirname);
    } else {
        if(chmod(dirname.c_str(), 0700)) {
            err(ss << "chmod failed: " << dirname);
        }
    }
}
