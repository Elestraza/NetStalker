#ifndef JOURNALHANDLER_H
#define JOURNALHANDLER_H

#include "Header.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

// Функции для работы с журналами
std::string getSystemDrive();
std::string readJournalFile(const std::filesystem::path& filePath);
std::vector<std::string> getUserProfiles(const std::string& ip);
void findJournals(const std::string& ip, std::ofstream& logFile);

#endif
