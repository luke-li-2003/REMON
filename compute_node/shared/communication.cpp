#include "communication.h"

communication::communication() : log("communication") {
    std::stringstream ss;
    socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == 0) {
        err(ss << "socket failed");
        setRunning(false);
    } else {
        setRunning(false);
    }
    ipStr = "unset";
}

communication::~communication() {

}

void communication::bufferReset() {
    memset(buffer, 0, REMON_BUFFER_SIZE);
}


int communication::safeSend(int socket, const char localBuffer[], size_t sendSize) {
    std::stringstream ss;
    int sendStatus = send(socket, localBuffer, sendSize, MSG_NOSIGNAL);
    if (sendStatus <= 0) {
        err(ss << "errno: " << strerror(errno));
        err(ss << "(send) lost connection with remote server: " << socket << " @ " << ipStr);
        close(socket);
        setRunning(false);
        bufferReset();
        return -1;
    }
    return sendStatus;
}
int communication::safeSend(int socket, int sendSize) {
    return safeSend(socket, buffer, sendSize);
}

int communication::safeSend() {
    return safeSend(socketFd, buffer, REMON_BUFFER_SIZE);
}

int communication::safeRead(int socket, char localBuffer[], size_t readSize) {
    std::stringstream ss;
    int valread = recv(socket, localBuffer, readSize, 0);
    if (valread <= 0) {
        err(ss << "errno: " << strerror(errno));
        err(ss << "(read) lost connection with remote peer: " << socket << " @ " << ipStr);
        close(socket);
        return -1;
    }
    return valread;
}

int communication::safeRead(int socket, int sendSize) {
    return safeRead(socket, buffer, sendSize);
}

int communication::safeRead() {
    return safeRead(socketFd, buffer, REMON_BUFFER_SIZE);
}