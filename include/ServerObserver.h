// Header guard
#ifndef SERVER_OBSERVER_H
#define SERVER_OBSERVER_H

#include <string>
#include <functional>
#include "Client.h"

struct ServerObserver
{
	std::string wantedIP = "";
    std::function<void(const std::string &clientIP,
                       const char *msg, size_t size)> incomingPacketHandler;

    std::function<void(const std::string &ip,
                       const std::string &msg)> disconnectionHandler;
};

#endif
