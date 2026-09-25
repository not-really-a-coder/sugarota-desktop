#pragma once
#include <string>
#include <vector>
#include <windows.h>

struct GlucoseEntry {
    double sgv = 0.0;
    std::string direction; // "Flat", "SingleUp", etc.
    long long date = 0;    // epoch ms
    double delta = 0.0;
};

struct AppConfig {
    std::wstring nightscoutUrl;
    std::wstring apiSecret;
    std::wstring unit = L"mmol"; // L"mmol" or L"mgdl"
    std::wstring theme = L"dark"; // L"dark" or L"light"
    int refreshInterval = 60;     // seconds
    bool showOnAllMonitors = true;
    int widgetCount = 1;          // min 1 max 5
    double alarmLow = 70.0;       // mg/dL
    double alarmHigh = 180.0;     // mg/dL
    int pillX = -1;
    int pillY = -1;
};

class ConfigManager {
public:
    static std::wstring GetConfigPath();
    static bool Load(AppConfig& config);
    static bool Save(const AppConfig& config);
};
