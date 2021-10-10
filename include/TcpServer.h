// Header guard
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <functional>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <mutex>

#include "Client.h"
#include "ServerObserver.h"
#include "PipeRet.h"
#include "FileDescriptor.h"

class TcpServer {
public:
    TcpServer();
    ~TcpServer();
    PipeRet start(int port, int maxNumOfClients = 5,
                  bool removeDeadClientsAutomatically = true);
    void initializeSocket();
    void bindAddress(int port);
    void listenToClients(int maxNumOfClients);
    std::string acceptClient(uint timeout);
    void subscribe(const ServerObserver & observer);
    PipeRet sendToAllClients(const char * msg, size_t size);
    PipeRet sendToClient(const std::string & clientIP,
                         const char * msg, size_t size);
    PipeRet close();
    void printClients();

private:
    FileDescriptor mSocketFd;
    struct sockaddr_in mServerAddress;
    struct sockaddr_in mClientAddress;
    fd_set mFds;
    std::vector<Client*> mClientsList;
    std::vector<ServerObserver> mSubscribersList;

    std::mutex mSubscribersMutex;
    std::mutex mClientsMutex;

    std::thread * mClientsRemoverThread = nullptr;
    std::atomic<bool> mStopRemoveClientsTask;

    void publishClientMsg(const Client & client,
                          const char * msg, size_t msgSize);

    void publishClientDisconnected(const std::string&,
                                   const std::string&);

    PipeRet waitForClient(uint32_t timeout);
    void clientEventHandler(const Client&, ClientEvent event,
                            const std::string &msg);

    void removeDeadClients();
    void terminateDeadClientsRemover();

    static PipeRet sendToClient(const Client & client,
                                const char * msg, size_t size);
};
#endif
