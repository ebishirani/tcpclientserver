#include "../include/TcpClient.h".h"
#include "../include/common.h"

TcpClient::TcpClient()
{
    mIsConnected = false;
    mIsClosed = true;
}
//---------------------------------------------------------------------------
TcpClient::~TcpClient()
{
    close();
}
//---------------------------------------------------------------------------
PipeRet TcpClient::connectTo(const std::string & address, int port)
{
    try
    {
        initializeSocket();
        setAddress(address, port);
    }
    catch (const std::runtime_error& error)
    {
        return PipeRet::failure(error.what());
    }

    const int connectResult = connect(
                mSocketFd.getSocketFd(),
                (struct sockaddr *)&mServer,
                sizeof(mServer));

    const bool connectionFailed = (connectResult == -1);
    if (connectionFailed)
    {
        return PipeRet::failure(strerror(errno));
    }

    startReceivingMessages();
    mIsConnected = true;
    mIsClosed = false;

    return PipeRet::success();
}
//---------------------------------------------------------------------------
void TcpClient::startReceivingMessages()
{
    mReceiveTask = new std::thread(&TcpClient::receiveTask, this);
}
//---------------------------------------------------------------------------
void TcpClient::initializeSocket()
{
    PipeRet ret;

    mSocketFd.setSocketFd(socket(AF_INET , SOCK_STREAM , 0));
    const bool socketFailed = (mSocketFd.getSocketFd() == -1);
    if (socketFailed)
    {
        throw std::runtime_error(strerror(errno));
    }
}
//---------------------------------------------------------------------------
void TcpClient::setAddress(const std::string& address, int port)
{
    const int inetSuccess = inet_aton(address.c_str(), &mServer.sin_addr);

    if(!inetSuccess) { // inet_addr failed to parse address
        // if hostname is not in IP strings and dots format, try resolve it
        struct hostent *host;
        struct in_addr **addrList;
        if ( (host = gethostbyname( address.c_str() ) ) == nullptr){
            throw std::runtime_error("Failed to resolve hostname");
        }
        addrList = (struct in_addr **) host->h_addr_list;
        mServer.sin_addr = *addrList[0];
    }
    mServer.sin_family = AF_INET;
    mServer.sin_port = htons(port);
}
//---------------------------------------------------------------------------
PipeRet TcpClient::sendMsg(const char * msg, size_t size)
{
    const size_t numBytesSent = send(mSocketFd.getSocketFd(), msg, size, 0);

    if (numBytesSent < 0 )
    { // send failed
        return PipeRet::failure(strerror(errno));
    }
    if (numBytesSent < size)
    { // not all bytes were sent
        char errorMsg[100];
        sprintf(errorMsg, "Only %lu bytes out of %lu was sent to client",
                numBytesSent, size);

        return PipeRet::failure(errorMsg);
    }
    return PipeRet::success();
}
//---------------------------------------------------------------------------
void TcpClient::subscribe(const ClientObserver & observer)
{
    std::lock_guard<std::mutex> lock(mSubscribersMtx);
    mSubscibersList.push_back(observer);
}
//---------------------------------------------------------------------------
/*
 * Publish incomingPacketHandler client message to observer.
 * Observers get only messages that originated
 * from clients with IP address identical to
 * the specific observer requested IP
 */
void TcpClient::publishServerMsg(const char * msg, size_t msgSize)
{
    std::lock_guard<std::mutex> lock(mSubscribersMtx);

    for (const auto &subscriber : mSubscibersList)
    {
        if (subscriber.incomingPacketHandler)
        {
            subscriber.incomingPacketHandler(msg, msgSize);
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
void TcpClient::publishServerDisconnected(const PipeRet & ret)
{
    std::lock_guard<std::mutex> lock(mSubscribersMtx);
    for (const auto &subscriber : mSubscibersList)
    {
        if (subscriber.disconnectionHandler)
        {
            subscriber.disconnectionHandler(ret);
        }
    }
}
//---------------------------------------------------------------------------
/*
 * Receive server packets, and notify user
 */
void TcpClient::receiveTask()
{
    while(mIsConnected)
    {
        const fd_wait::Result waitResult = fd_wait::waitFor(mSocketFd);

        if (waitResult == fd_wait::Result::kFailure)
        {
            throw std::runtime_error(strerror(errno));
        } else if (waitResult == fd_wait::Result::kTimeout)
        {
            continue;
        }

        char msg[MAX_PACKET_SIZE];
        const size_t numOfBytesReceived =
                recv(mSocketFd.getSocketFd(), msg, MAX_PACKET_SIZE, 0);

        if(numOfBytesReceived < 1)
        {
            std::string errorMsg;
            if (numOfBytesReceived == 0)
            { //server closed connection
                errorMsg = "Server closed connection";
            }
            else
            {
                errorMsg = strerror(errno);
            }
            mIsConnected = false;
            publishServerDisconnected(PipeRet::failure(errorMsg));
            return;
        }
        else
        {
            publishServerMsg(msg, numOfBytesReceived);
        }
    }
}
//---------------------------------------------------------------------------
void TcpClient::terminateReceiveThread()
{
    mIsConnected = false;

    if (mReceiveTask)
    {
        mReceiveTask->join();
        delete mReceiveTask;
        mReceiveTask = nullptr;
    }
}
//---------------------------------------------------------------------------
PipeRet TcpClient::close()
{
    if (mIsClosed)
    {
        return PipeRet::failure("client is already closed");
    }
    terminateReceiveThread();

    const bool closeFailed = (::close(mSocketFd.getSocketFd()) == -1);
    if (closeFailed)
    {
        return PipeRet::failure(strerror(errno));
    }
    mIsClosed = true;
    return PipeRet::success();
}
//---------------------------------------------------------------------------

