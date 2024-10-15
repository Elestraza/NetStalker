#include "NetworkHandler.h"
#include "JournalHandler.h"
#include "Header.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Функция проверки VNC
void checkVNC(const char* ip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed for " << ip << ": " << WSAGetLastError() << std::endl;
        return;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(5900); // VNC default port
    inet_pton(AF_INET, ip, &server.sin_addr);

    DWORD timeout = 1000; // 1 second
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    if (connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
        std::cout << "VNC server found: " << ip << std::endl;

        std::ofstream logFile("journal_log.log", std::ios::app);
        findJournals(ip, logFile); // Log journals if VNC server is found
        logFile.close();
    }
    else {
        std::cerr << "Failed to connect to VNC server at: " << ip << std::endl;
    }

    closesocket(sock);
}

// Функция для получения шлюзов
std::vector<std::string> getGateways() {
    std::vector<std::string> gateways;

    ULONG bufferSize = 15000; // Initial size
    PIP_ADAPTER_ADDRESSES adapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
    if (adapterAddresses == nullptr) {
        std::cerr << "Memory allocation failed for adapterAddresses." << std::endl;
        return gateways;
    }

    ULONG result = GetAdaptersAddresses(AF_INET, 0, NULL, adapterAddresses, &bufferSize);
    if (result == ERROR_BUFFER_OVERFLOW) {
        free(adapterAddresses);
        adapterAddresses = (PIP_ADAPTER_ADDRESSES)malloc(bufferSize);
        result = GetAdaptersAddresses(AF_INET, 0, NULL, adapterAddresses, &bufferSize);
    }

    if (result == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES adapter = adapterAddresses; adapter != NULL; adapter = adapter->Next) {
            std::wcout << L"Adapter name: " << adapter->AdapterName << std::endl;
            std::wcout << L"Adapter description: " << adapter->Description << std::endl;

            if (adapter->FirstGatewayAddress != nullptr) {
                std::cout << "Gateways for this adapter:" << std::endl;
                IP_ADAPTER_GATEWAY_ADDRESS* gateway = adapter->FirstGatewayAddress;
                while (gateway != nullptr) {
                    sockaddr_in* gatewayAddr = reinterpret_cast<sockaddr_in*>(gateway->Address.lpSockaddr);
                    if (gatewayAddr->sin_family == AF_INET) {
                        char gatewayIP[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(gatewayAddr->sin_addr), gatewayIP, INET_ADDRSTRLEN);
                        std::cout << "Found gateway: " << gatewayIP << std::endl;
                        gateways.push_back(gatewayIP);
                    }
                    gateway = gateway->Next;
                }
            }
            else {
                std::cout << "No gateways found for this adapter." << std::endl;
            }
        }
    }
    else {
        std::cerr << "Failed to retrieve gateway addresses. Error: " << result << std::endl;
    }

    free(adapterAddresses);
    return gateways;
}

// Функция поиска VNC серверов
void findVNCServersInNetwork() {
    std::vector<std::thread> threads;

    // Получаем IP-адреса и шлюзы
    std::vector<std::string> gateways = getGateways();
    if (gateways.empty()) {
        std::cerr << "No gateways found!" << std::endl;
        //return;
    }

    for (const std::string& gateway : gateways) {
        std::string subnet = gateway.substr(0, gateway.find_last_of('.') + 1);

        // Перебираем все возможные IP в подсети
        for (int i = 1; i < 255; ++i) {
            std::string ip = subnet + std::to_string(i);
            threads.emplace_back(checkVNC, ip.c_str());
        }
    }

    // Ждем завершения всех потоков
    for (auto& t : threads) {
        t.join();
    }
}
