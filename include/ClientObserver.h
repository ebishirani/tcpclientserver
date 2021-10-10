// Header guard
#ifndef CLIENT_OBSERVER_H
#define CLIENT_OBSERVER_H

#include <string>
#include <functional>
#include "PipeRet.h"

struct ClientObserver
{
    std::string wantedIP = "";
    std::function<void(const char * msg, size_t size)>
        incomingPacketHandler = nullptr;

    std::function<void(const PipeRet & ret)>
        disconnectionHandler = nullptr;
};
#endif

