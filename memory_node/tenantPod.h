#ifndef MEMORY_NODE_TENANT_POD_H
#define MEMORY_NODE_TENANT_POD_H

#include <iostream>
#include <sys/mman.h>

using namespace std;

class tenantPod {
private:
    char* payload;
    size_t pageSize;
public:
    tenantPod(size_t pageSize){
        this->pageSize = pageSize;
        payload = (char*) mmap(nullptr, pageSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
        if(payload == MAP_FAILED) {
            perror("map");
            std::cout << "Also check your /proc/sys/vm/max_map_count" << std::endl;
            exit(-1);
        }
    };
    ~tenantPod() {
        munmap(payload, pageSize);
    };
    char* getPayload() {return payload;};
    size_t getPageSize() {
        return pageSize;
    }
};


#endif //MEMORY_NODE_TENANT_POD_H
