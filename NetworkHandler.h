#ifndef NETWORKHANDLER_H
#define NETWORKHANDLER_H

#include "Header.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Функции для работы с сетью
std::vector<std::string> getGateways();
void checkVNC(const char* ip);
void findVNCServersInNetwork();

#endif
