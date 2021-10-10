// Header guard
#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netdb.h>
#include <vector>
#include <errno.h>
#include <thread>
#include <mutex>
#include <atomic>
#include "ClientObserver.h"
#include "PipeRet.h"
#include "FileDescriptor.h"


class TcpClient
{
public:
    TcpClient();
    ~TcpClient();
    PipeRet connectTo(const std::string & address, int port);
    PipeRet sendMsg(const char * msg, size_t size);

    void subscribe(const ClientObserver & observer);
    bool isConnected() const
    {
        return mIsConnected;
    }
    PipeRet close();

private:
    FileDescriptor mSocketFd;
    std::atomic<bool> mIsConnected;
    std::atomic<bool> mIsClosed;
    struct sockaddr_in mServer;
    std::vector<ClientObserver> mSubscibersList;
    std::thread * mReceiveTask = nullptr;
    std::mutex mSubscribersMtx;

    void initializeSocket();
    void startReceivingMessages();
    void setAddress(const std::string& address, int port);
    void publishServerMsg(const char * msg, size_t msgSize);
    void publishServerDisconnected(const PipeRet & ret);
    void receiveTask();
    void terminateReceiveThread();
};

#endif
