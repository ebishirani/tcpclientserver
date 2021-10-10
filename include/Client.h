// Header guard
#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <thread>
#include <functional>
#include <mutex>
#include <atomic>

#include "PipeRet.h"
#include "ClientEvent.h"
#include "FileDescriptor.h"


class Client
{

    using clientEventHandler = std::function<void(
                                       const Client&,
                                       ClientEvent,
                                       const std::string&)>;
public:
    Client(int);

    bool operator ==(const Client & other) const ;

    void setIp(const std::string & ip)
    {
        mIp = ip;
    }
    std::string getIp() const
    {
        return mIp;
    }

    void setEventsHandler(const clientEventHandler & eventHandler)
    {
        mEventHandlerCallback = eventHandler;
    }
    void publishEvent(ClientEvent clientEvent, const std::string &msg = "");

    bool isConnected() const
    {
        return mIsConnected;
    }

    void startListen();

    void send(const char * msg, size_t msgSize) const;

    void close();

    void print() const;

private:
    FileDescriptor mSocketFd;
    std::string mIp = "";
    std::atomic<bool> mIsConnected;
    std::thread * mReceiveThread = nullptr;
    clientEventHandler mEventHandlerCallback;

    void setConnected(bool flag)
    {
        mIsConnected = flag;
    }

    void receiveTask();

    void terminateReceiveThread();

};
#endif
