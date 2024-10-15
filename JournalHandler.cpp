#include "JournalHandler.h"
#include "Header.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace fs = std::filesystem;

// Функция для получения значения переменной окружения %SystemDrive%
std::string getSystemDrive() {
    char* systemDrive = nullptr;
    size_t len;
    errno_t err = _dupenv_s(&systemDrive, &len, "SystemDrive");
    if (err != 0 || systemDrive == nullptr) {
        std::cerr << "Failed to get %SystemDrive%" << std::endl;
        return std::string(); // Возвращаем пустую строку в случае ошибки
    }

    std::string result(systemDrive);
    free(systemDrive); // Освобождаем память, выделенную _dupenv_s
    return result;
}

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

    try {
        std::string systemDrive = getSystemDrive();
        if (systemDrive.empty()) {
            return userProfiles;
        }

        // Собираем путь к директории пользователей
        fs::path userProfilesPath = fs::path("\\\\") / ip / (systemDrive + "$") / "Users";

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

        std::string systemDrive = getSystemDrive();
        if (systemDrive.empty()) {
            return;
        }

        for (const auto& profile : userProfiles) {
            for (const auto& location : journalLocations) {
                fs::path journalPath = fs::path("\\\\") / ip / (systemDrive + "$") / "Users" / profile / location;

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
