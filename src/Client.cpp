#include <cstdio>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <stdexcept>
#include <sys/socket.h>
#include <iostream>

#include "../include/Client.h"
#include "../include/common.h"

Client::Client(int fileDescriptor)
{
    mSocketFd.setSocketFd(fileDescriptor);
    setConnected(false);
}
//---------------------------------------------------------------------------
bool Client::operator==(const Client & other) const
{
    if ((this->mSocketFd.getSocketFd() == other.mSocketFd.getSocketFd()) &&
        (this->mIp == other.mIp) )
    {
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------
void Client::startListen()
{
    setConnected(true);
    mReceiveThread = new std::thread(&Client::receiveTask, this);
}
//---------------------------------------------------------------------------
void Client::send(const char *msg, size_t msgSize) const
{
    const size_t numBytesSent = ::send(mSocketFd.getSocketFd(),
                                       (char *)msg, msgSize, 0);

    const bool sendFailed = (numBytesSent < 0);
    if (sendFailed)
    {
        throw std::runtime_error(strerror(errno));
    }

    const bool notAllBytesWereSent = (numBytesSent < msgSize);
    if (notAllBytesWereSent)
    {
        char errorMsg[100];
        sprintf(errorMsg, "Only %lu bytes out of %lu was sent to client",
                numBytesSent, msgSize);
        throw std::runtime_error(errorMsg);
    }
}
//---------------------------------------------------------------------------
/*
 * Receive client packets, and notify user
 */
void Client::receiveTask()
{
    while(isConnected())
    {
        const fd_wait::Result waitResult = fd_wait::waitFor(mSocketFd);

        if (waitResult == fd_wait::Result::kFailure)
        {
            throw std::runtime_error(strerror(errno));
        }
        else if (waitResult == fd_wait::Result::kTimeout)
        {
            continue;
        }

        char receivedMessage[MAX_PACKET_SIZE] = {'\0',};
        const size_t numOfBytesReceived = recv(
                    mSocketFd.getSocketFd(),
                    receivedMessage,
                    MAX_PACKET_SIZE,
                    0);

        if(numOfBytesReceived < 1)
        {
            const bool clientClosedConnection = (numOfBytesReceived == 0);
            std::string disconnectionMessage;
            if (clientClosedConnection)
            {
                disconnectionMessage = "Client closed connection";
            }
            else
            {
                disconnectionMessage = strerror(errno);
            }
            setConnected(false);
            publishEvent(ClientEvent::kDisconnected, disconnectionMessage);
            return;
        }
        else
        {
            publishEvent(ClientEvent::kIncommingMsg, receivedMessage);
        }
    }
}
//---------------------------------------------------------------------------
void Client::publishEvent(ClientEvent clientEvent, const std::string &msg)
{
    mEventHandlerCallback(*this, clientEvent, msg);
}
//---------------------------------------------------------------------------
void Client::print() const
{
    const std::string connected = isConnected() ? "True" : "False";
    std::cout << "-----------------\n" <<
              "IP address: " << getIp() << std::endl <<
              "Connected?: " << connected << std::endl <<
              "Socket FD: " << mSocketFd.getSocketFd() << std::endl;
}
//---------------------------------------------------------------------------
void Client::terminateReceiveThread()
{
    setConnected(false);
    if (mReceiveThread)
    {
        mReceiveThread->join();
        delete mReceiveThread;
        mReceiveThread = nullptr;
    }
}
//---------------------------------------------------------------------------
void Client::close()
{
    terminateReceiveThread();

    const bool closeFailed = (::close(mSocketFd.getSocketFd()) == -1);
    if (closeFailed)
    {
        throw std::runtime_error(strerror(errno));
    }
}

