#include "config.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <iostream>

static std::string WStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &strTo[0], sizeNeeded, NULL, NULL);
    return strTo;
}

static std::wstring Utf8ToWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), &wstrTo[0], sizeNeeded);
    return wstrTo;
}

std::wstring ConfigManager::GetConfigPath() {
    PWSTR appDataPath = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &appDataPath))) {
        std::wstring dir = std::wstring(appDataPath) + L"\\sugarota-desktop";
        CreateDirectoryW(dir.c_str(), NULL);
        std::wstring path = dir + L"\\config.json";
        CoTaskMemFree(appDataPath);
        return path;
    }
    return L"config.json";
}

bool ConfigManager::Load(AppConfig& config) {
    std::wstring path = GetConfigPath();
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        auto trim = [](std::string& s) {
            size_t p1 = s.find_first_not_of(" \t\r\n\",");
            size_t p2 = s.find_last_not_of(" \t\r\n\",");
            if (p1 == std::string::npos) s = "";
            else s = s.substr(p1, p2 - p1 + 1);
        };
        trim(key);
        trim(val);

        if (key == "nightscoutUrl") config.nightscoutUrl = Utf8ToWString(val);
        else if (key == "apiSecret") config.apiSecret = Utf8ToWString(val);
        else if (key == "unit") config.unit = Utf8ToWString(val);
        else if (key == "theme") config.theme = Utf8ToWString(val);
        else if (key == "refreshInterval") config.refreshInterval = std::stoi(val);
        else if (key == "showOnAllMonitors") config.showOnAllMonitors = (val == "true" || val == "1");
        else if (key == "widgetCount") {
            try { config.widgetCount = (std::max)(1, (std::min)(5, std::stoi(val))); } catch (...) {}
        }
        else if (key == "alarmLow") config.alarmLow = std::stod(val);
        else if (key == "alarmHigh") config.alarmHigh = std::stod(val);
        else if (key == "pillX") config.pillX = std::stoi(val);
        else if (key == "pillY") config.pillY = std::stoi(val);
    }
    return true;
}

bool ConfigManager::Save(const AppConfig& config) {
    std::wstring path = GetConfigPath();
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "{\n";
    file << "  \"nightscoutUrl\": \"" << WStringToUtf8(config.nightscoutUrl) << "\",\n";
    file << "  \"apiSecret\": \"" << WStringToUtf8(config.apiSecret) << "\",\n";
    file << "  \"unit\": \"" << WStringToUtf8(config.unit) << "\",\n";
    file << "  \"theme\": \"" << WStringToUtf8(config.theme) << "\",\n";
    file << "  \"refreshInterval\": " << config.refreshInterval << ",\n";
    file << "  \"showOnAllMonitors\": " << (config.showOnAllMonitors ? "true" : "false") << ",\n";
    file << "  \"widgetCount\": " << config.widgetCount << ",\n";
    file << "  \"alarmLow\": " << config.alarmLow << ",\n";
    file << "  \"alarmHigh\": " << config.alarmHigh << ",\n";
    file << "  \"pillX\": " << config.pillX << ",\n";
    file << "  \"pillY\": " << config.pillY << "\n";
    file << "}\n";
    return true;
}
