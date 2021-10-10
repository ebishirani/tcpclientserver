#include <functional>
#include <thread>
#include <algorithm>
#include "../include/TcpServer.h"
#include "../include/common.h"

TcpServer::TcpServer()
{
    mSubscribersList.reserve(10);
    mClientsList.reserve(10);
    mStopRemoveClientsTask = false;
}
//---------------------------------------------------------------------------
TcpServer::~TcpServer()
{
    close();
}
//---------------------------------------------------------------------------
void TcpServer::subscribe(const ServerObserver & observer)
{
    std::lock_guard<std::mutex> lock(mSubscribersMutex);
    mSubscribersList.push_back(observer);
}
//---------------------------------------------------------------------------
void TcpServer::printClients()
{
    std::lock_guard<std::mutex> lock(mClientsMutex);
    if (mClientsList.empty())
    {
        std::cout << "no connected clients\n";
    }
    for (const Client *client : mClientsList)
    {
        client->print();
    }
}
//---------------------------------------------------------------------------
/**
 * Remove dead clients (disconnected) from clients vector periodically
 */
void TcpServer::removeDeadClients()
{
    std::vector<Client*>::const_iterator clientToRemove;
    while (!mStopRemoveClientsTask)
    {
        {
            std::lock_guard<std::mutex> lock(mClientsMutex);
            do
            {
                clientToRemove = std::find_if(
                            mClientsList.begin(),
                            mClientsList.end(),
                            [](Client *client)
                            {
                                 return !client->isConnected();
                            });

                if (clientToRemove != mClientsList.end())
                {
                    (*clientToRemove)->close();
                    delete *clientToRemove;
                    mClientsList.erase(clientToRemove);
                }
            } while (clientToRemove != mClientsList.end());
        }
        sleep(2);
    }
}
//---------------------------------------------------------------------------
void TcpServer::terminateDeadClientsRemover()
{
    if (mClientsRemoverThread)
    {
        mStopRemoveClientsTask = true;
        mClientsRemoverThread->join();
        delete mClientsRemoverThread;
        mClientsRemoverThread = nullptr;
    }
}
//---------------------------------------------------------------------------
/**
 * Handle different client events. Subscriber callbacks should be short and
 * fast, and must not
 * call other server functions to avoid deadlock
 */
void TcpServer::clientEventHandler(
        const Client &client,
        ClientEvent event,
        const std::string &msg)
{
    switch (event)
    {
        case ClientEvent::kDisconnected:
        {
            publishClientDisconnected(client.getIp(), msg);
            break;
        }
        case ClientEvent::kIncommingMsg:
        {
            publishClientMsg(client, msg.c_str(), msg.size());
            break;
        }
    }
}
//---------------------------------------------------------------------------
/*
 * Publish incomingPacketHandler client message to observer.
 * Observers get only messages that originated
 * from clients with IP address identical to
 * the specific observer requested IP
 */
void TcpServer::publishClientMsg(
        const Client & client,
        const char * msg,
        size_t msgSize)
{
    std::lock_guard<std::mutex> lock(mSubscribersMutex);

    for (const ServerObserver& subscriber : mSubscribersList)
    {
        if (subscriber.wantedIP == client.getIp() ||
                subscriber.wantedIP.empty())
        {
            if (subscriber.incomingPacketHandler)
            {
                subscriber.incomingPacketHandler(client.getIp(), msg,
                                                 msgSize);
            }
        }
    }
}
//---------------------------------------------------------------------------
/*
 * Publish client disconnection to observer.
 * Observers get only notify about clients
 * with IP address identical to the specific
 * observer requested IP
 */
void TcpServer::publishClientDisconnected(
        const std::string &clientIP,
        const std::string &clientMsg)
{
    std::lock_guard<std::mutex> lock(mSubscribersMutex);

    for (const ServerObserver& subscriber : mSubscribersList)
    {
        if (subscriber.wantedIP == clientIP)
        {
            if (subscriber.disconnectionHandler)
            {
                subscriber.disconnectionHandler(clientIP, clientMsg);
            }
        }
    }
}
//---------------------------------------------------------------------------
/*
 * Bind port and start listening
 * Return tcp_ret_t
 */
PipeRet TcpServer::start(
        int port,
        int maxNumOfClients,
        bool removeDeadClientsAutomatically)
{
    if (removeDeadClientsAutomatically)
    {
        mClientsRemoverThread = new std::thread(
                    &TcpServer::removeDeadClients,
                    this);
    }
    try
    {
        initializeSocket();
        bindAddress(port);
        listenToClients(maxNumOfClients);
    }
    catch (const std::runtime_error &error)
    {
        return PipeRet::failure(error.what());
    }
    return PipeRet::success();
}
//---------------------------------------------------------------------------
void TcpServer::initializeSocket()
{
    mSocketFd.setSocketFd(socket(AF_INET, SOCK_STREAM, 0));
    const bool socketFailed = (mSocketFd.getSocketFd() == -1);
    if (socketFailed)
    {
        throw std::runtime_error(strerror(errno));
    }

    // set socket for reuse (otherwise might have to wait 4 minutes every
    //time socket is closed)
    const int option = 1;
    setsockopt(mSocketFd.getSocketFd(),
               SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
}
//---------------------------------------------------------------------------
void TcpServer::bindAddress(int port)
{
    memset(&mServerAddress, 0, sizeof(mServerAddress));
    mServerAddress.sin_family = AF_INET;
    mServerAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    mServerAddress.sin_port = htons(port);

    const int bindResult = bind(mSocketFd.getSocketFd(),
                                (struct sockaddr *)&mServerAddress,
                                sizeof(mServerAddress));

    const bool bindFailed = (bindResult == -1);
    if (bindFailed)
    {
        throw std::runtime_error(strerror(errno));
    }
}
//---------------------------------------------------------------------------
void TcpServer::listenToClients(int maxNumOfClients)
{
    const int clientsQueueSize = maxNumOfClients;
    const bool listenFailed = (listen(mSocketFd.getSocketFd(),
                                      clientsQueueSize) == -1);
    if (listenFailed)
    {
        throw std::runtime_error(strerror(errno));
    }
}
//---------------------------------------------------------------------------
/*
 * Accept and handle new client socket. To handle multiple clients, user must
 * call this function in a loop to enable the acceptance of more than one.
 * If timeout argument equal 0, this function is executed in blocking mode.
 * If timeout argument is > 0 then this function is executed in non-blocking
 * mode (async) and will quit after timeout seconds if no client tried to
 * connect.
 * Return accepted client IP, or throw error if failed
 */
std::string TcpServer::acceptClient(uint timeout)
{
    const PipeRet waitingForClient = waitForClient(timeout);
    if (!waitingForClient.isSuccessful())
    {
        throw std::runtime_error(waitingForClient.message());
    }

    socklen_t socketSize  = sizeof(mClientAddress);
    const int fileDescriptor = accept(mSocketFd.getSocketFd(),
                                      (struct sockaddr*)&mClientAddress,
                                      &socketSize);

    const bool acceptFailed = (fileDescriptor == -1);
    if (acceptFailed)
    {
        throw std::runtime_error(strerror(errno));
    }

    auto newClient = new Client(fileDescriptor);
    newClient->setIp(inet_ntoa(mClientAddress.sin_addr));
    using namespace std::placeholders;
    newClient->setEventsHandler(
                std::bind(&TcpServer::clientEventHandler, this, _1, _2, _3));
    newClient->startListen();

    std::lock_guard<std::mutex> lock(mClientsMutex);
    mClientsList.push_back(newClient);

    return newClient->getIp();
}
//---------------------------------------------------------------------------
PipeRet TcpServer::waitForClient(uint32_t timeout)
{
    if (timeout > 0) {
        const fd_wait::Result waitResult =
                fd_wait::waitFor(mSocketFd, timeout);

        const bool noIncomingClient =
                (!FD_ISSET(mSocketFd.getSocketFd(), &mFds));

        if (waitResult == fd_wait::Result::kFailure)
        {
            return PipeRet::failure(strerror(errno));
        }
        else if (waitResult == fd_wait::Result::kTimeout)
        {
            return PipeRet::failure("Timeout waiting for client");
        }
        else if (noIncomingClient)
        {
            return PipeRet::failure("File descriptor is not set");
        }
    }

    return PipeRet::success();
}
//---------------------------------------------------------------------------
/*
 * Send message to all connected clients.
 * Return true if message was sent successfully to all clients
 */
PipeRet TcpServer::sendToAllClients(const char * msg, size_t size)
{
    std::lock_guard<std::mutex> lock(mClientsMutex);

    for (const Client *client : mClientsList)
    {
        PipeRet sendingResult = sendToClient(*client, msg, size);
        if (!sendingResult.isSuccessful())
        {
            return sendingResult;
        }
    }
    return PipeRet::success();
}
//---------------------------------------------------------------------------
/*
 * Send message to specific client (determined by client IP address).
 * Return true if message was sent successfully
 */
PipeRet TcpServer::sendToClient(
        const Client & client,
        const char * msg,
        size_t size)
{
    try
    {
        client.send(msg, size);
    }
    catch (const std::runtime_error &error)
    {
        return PipeRet::failure(error.what());
    }

    return PipeRet::success();
}
//---------------------------------------------------------------------------
PipeRet TcpServer::sendToClient(
        const std::string & clientIP,
        const char * msg, size_t size)
{
    std::lock_guard<std::mutex> lock(mClientsMutex);

    const auto clientIter = std::find_if(mClientsList.begin(),
                                         mClientsList.end(),
         [&clientIP](Client *client){ return client->getIp() == clientIP;});

    if (clientIter == mClientsList.end())
    {
        return PipeRet::failure("client not found");
    }

    const Client &client = *(*clientIter);
    return sendToClient(client, msg, size);
}
//---------------------------------------------------------------------------
/*
 * Close server and clients resources.
 * Return true is successFlag, false otherwise
 */
PipeRet TcpServer::close()
{
    terminateDeadClientsRemover();
    { // close clients
        std::lock_guard<std::mutex> lock(mClientsMutex);

        for (Client * client : mClientsList)
        {
            try
            {
                client->close();
            }
            catch (const std::runtime_error& error)
            {
                return PipeRet::failure(error.what());
            }
        }
        mClientsList.clear();
    }

    { // close server
        const int closeServerResult = ::close(mSocketFd.getSocketFd());
        const bool closeServerFailed = (closeServerResult == -1);
        if (closeServerFailed)
        {
            return PipeRet::failure(strerror(errno));
        }
    }

    return PipeRet::success();
}
//---------------------------------------------------------------------------
