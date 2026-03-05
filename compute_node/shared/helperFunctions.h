#ifndef REMON_HELPER_FUNCTIONS_H
#define REMON_HELPER_FUNCTIONS_H

#include <string>
#include <vector>
#include <sstream>
#include <sys/socket.h>
#include <cstdlib>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <fstream>
#include <list>
#include <cstdio>

#include "define.h"
#include "sys/resource.h"

std::vector<std::string> tokenizeString(std::string strIn);

std::string shExec(const char *cmd);

void printStatm();

std::string computeQuickHash(unsigned char* ptr, size_t size);

size_t pointerMinusPointer(void* a, void* b);
void* pointerPlusOffset(void *a, size_t offset);

bool liftUlimit(size_t size);


#endif //REMON_HELPER_FUNCTIONS_H
