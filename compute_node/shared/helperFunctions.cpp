#include <iomanip>
#include "helperFunctions.h"

std::vector<std::string> tokenizeString(std::string strIn) {
    std::stringstream ss(strIn);
    std::string str;
    std::vector<std::string> result;
    while (ss >> str) {
        result.push_back(str);
    }
    return result;
}


std::string shExec(const char *cmd) {
    std::array<char, REMON_BUFFER_SIZE> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void printStatm() {
    std::ifstream fh("/proc/self/statm");
    std::string line;
    std::cout << "helper print_statm:";
    while (getline(fh, line)) {
        std::cout << line << "\n";
    }
    std::cout << std::endl;
}

std::string computeQuickHash(unsigned char* ptr, size_t size) {
    u_int8_t masks[7] = {0b10101010, 0b01010101, 0b11001100, 0b00110011, 0b11100000, 0b00000111, 0b10011001};
    u_int8_t buffer[32] = {0};
    std::string words = "0123456789abcdefghijklmnopqrstuvwxyz";
    for(int i = 0; i<size;i++) {
        u_int8_t ptrCurr = ptr[i];
        ptrCurr ^= masks[i%7];
        buffer[i % 32] ^= ptrCurr;
    }
    std::stringstream ss;
    for(int i = 0; i < 32; i++) {
        unsigned index = buffer[i] % words.size();
        ss << words[index];
    }
    return ss.str();
}

size_t pointerMinusPointer(void *a, void *b) {
    return reinterpret_cast<uint8_t*>(a) - reinterpret_cast<uint8_t*>(b);
}

void* pointerPlusOffset(void *a, size_t offset) {
    return reinterpret_cast<uint8_t*>(a) + offset;
}


bool liftUlimit(size_t size){
    std::cout << "lift ulimit NOFILE: " << size << std::endl;
    if(size < MIN_FD_COUNT) {
        size = 1024;
    }
    struct rlimit rlp;
    rlp.rlim_cur = size;
    rlp.rlim_max = size;

    if(setrlimit(RLIMIT_NOFILE, &rlp) == -1) {
        perror(("cannot lift ulimit -n " + std::to_string(size)).c_str());
        return false;
    }
    return true;
}

