#include "nightscout.h"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <regex>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

static bool ParseEntries(const std::string& json, std::vector<GlucoseEntry>& entries) {
    entries.clear();

    // Scan through all occurrences of "sgv"
    size_t pos = 0;
    while (pos < json.length()) {
        size_t sgvKey = json.find("\"sgv\"", pos);
        if (sgvKey == std::string::npos) break;

        // Find surrounding curly braces { ... }
        size_t objStart = json.rfind('{', sgvKey);
        size_t objEnd = json.find('}', sgvKey);
        if (objStart == std::string::npos || objEnd == std::string::npos || objStart < pos) {
            pos = sgvKey + 5;
            continue;
        }

        std::string objStr = json.substr(objStart, objEnd - objStart + 1);

        GlucoseEntry e;
        // Extract SGV
        std::regex sgvRegex(R"raw("sgv"\s*:\s*([0-9\.]+))raw");
        std::smatch sgvMatch;
        if (std::regex_search(objStr, sgvMatch, sgvRegex)) {
            try { e.sgv = std::stod(sgvMatch[1].str()); } catch (...) { e.sgv = 0.0; }
        }

        if (e.sgv > 0.0) {
            // Direction
            std::regex dirRegex(R"raw("direction"\s*:\s*"([^"]+)")raw");
            std::smatch dirMatch;
            if (std::regex_search(objStr, dirMatch, dirRegex)) {
                e.direction = dirMatch[1].str();
            } else {
                e.direction = "Flat";
            }

            // Date (numeric millisecond timestamp or string)
            std::regex dateRegex(R"raw("date"\s*:\s*([0-9]{10,14}))raw");
            std::smatch dateMatch;
            if (std::regex_search(objStr, dateMatch, dateRegex)) {
                try { e.date = std::stoll(dateMatch[1].str()); } catch (...) { e.date = 0; }
            }
            if (e.date == 0) {
                // Try dateString / sysTime / created_at timestamp
                std::regex isoRegex(R"raw("(?:dateString|sysTime|created_at)"\s*:\s*"([^"]+)")raw");
                std::smatch isoMatch;
                if (std::regex_search(objStr, isoMatch, isoRegex)) {
                    // Approximate parse ISO: YYYY-MM-DDTHH:MM:SS
                    int y = 0, m = 0, d = 0, h = 0, min = 0, sec = 0;
                    std::string is = isoMatch[1].str();
                    if (sscanf_s(is.c_str(), "%d-%d-%dT%d:%d:%d", &y, &m, &d, &h, &min, &sec) >= 5) {
                        struct tm tmStruct = { 0 };
                        tmStruct.tm_year = y - 1900;
                        tmStruct.tm_mon = m - 1;
                        tmStruct.tm_mday = d;
                        tmStruct.tm_hour = h;
                        tmStruct.tm_min = min;
                        tmStruct.tm_sec = sec;
                        time_t t = _mkgmtime(&tmStruct);
                        if (t > 0) e.date = (long long)t * 1000LL;
                    }
                }
            }

            // Delta
            std::regex deltaRegex(R"raw("delta"\s*:\s*([\-0-9\.]+))raw");
            std::smatch deltaMatch;
            if (std::regex_search(objStr, deltaMatch, deltaRegex)) {
                try { e.delta = std::stod(deltaMatch[1].str()); } catch (...) { e.delta = 0.0; }
            }

            entries.push_back(e);
            if (entries.size() >= 36) break;
        }

        pos = objEnd + 1;
    }

    // Always compute delta from adjacent entry if missing or 0
    if (entries.size() > 1 && entries[0].delta == 0.0) {
        entries[0].delta = entries[0].sgv - entries[1].sgv;
    }

    return !entries.empty();
}

static bool HttpGet(const std::wstring& fullUrl, const std::wstring& token, std::string& outResponse, std::string& outError) {
    URL_COMPONENTS urlComp = { sizeof(urlComp) };
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(fullUrl.c_str(), (DWORD)fullUrl.length(), 0, &urlComp)) {
        outError = "Invalid URL";
        return false;
    }

    std::wstring host(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring path(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    HINTERNET hSession = WinHttpOpen(L"Sugarota-Desktop-Native/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        outError = "Failed to open WinHTTP session";
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        outError = "Failed to connect to host";
        return false;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "Failed to open HTTP request";
        return false;
    }

    std::wstring headers = L"Accept: application/json\r\n";
    if (!token.empty()) {
        headers += L"api-secret: " + token + L"\r\n";
    }

    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.length(),
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (!bResults) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "HTTP Request failed (code " + std::to_string(GetLastError()) + ")";
        return false;
    }

    DWORD dwStatusCode = 0;
    DWORD dwSize = sizeof(dwStatusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

    if (dwStatusCode != 200) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        outError = "HTTP Status " + std::to_string(dwStatusCode);
        return false;
    }

    std::string response;
    DWORD dwDownloaded = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;

        std::vector<char> buf(dwSize + 1);
        if (WinHttpReadData(hRequest, buf.data(), dwSize, &dwDownloaded)) {
            response.append(buf.data(), dwDownloaded);
        }
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    outResponse = response;
    return true;
}

NightscoutClient::NightscoutClient() {}
NightscoutClient::~NightscoutClient() { Stop(); }

void NightscoutClient::Start(const AppConfig& config, Callback callback) {
    Stop();
    m_config = config;
    m_callback = callback;
    m_running = true;
    m_thread = std::thread(&NightscoutClient::WorkerLoop, this);
}

void NightscoutClient::Stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void NightscoutClient::FetchNow() {
    m_forceFetch = true;
}

void NightscoutClient::UpdateConfig(const AppConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    m_forceFetch = true;
}

bool NightscoutClient::PerformFetch(std::vector<GlucoseEntry>& entries, std::string& error) {
    std::wstring url;
    std::wstring token;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        url = m_config.nightscoutUrl;
        token = m_config.apiSecret;
    }

    if (url.empty()) {
        error = "Please enter Nightscout URL in Settings";
        return false;
    }

    // Build endpoint: normalize URL
    if (url.rfind(L"http", 0) != 0) {
        url = L"https://" + url;
    }
    while (!url.empty() && url.back() == L'/') {
        url.pop_back();
    }

    // Strip any existing count=... from the URL
    size_t countPos = url.find(L"count=");
    if (countPos != std::wstring::npos) {
        size_t ampPos = url.find(L'&', countPos);
        size_t qPos = (countPos > 0 && url[countPos - 1] == L'?') ? (countPos - 1) : countPos;
        if (ampPos != std::wstring::npos) {
            url.erase(countPos, ampPos - countPos + 1);
        } else {
            url.erase(qPos);
        }
    }

    if (url.find(L"/api/v1/entries") == std::wstring::npos) {
        url += L"/api/v1/entries.json?count=36";
    } else {
        url += (url.find(L'?') == std::wstring::npos ? L"?count=36" : L"&count=36");
    }

    if (!token.empty()) {
        url += (url.find(L'?') == std::wstring::npos ? L"?token=" : L"&token=") + token;
    }

    std::string response;
    if (!HttpGet(url, token, response, error)) {
        return false;
    }

    return ParseEntries(response, entries);
}

void NightscoutClient::WorkerLoop() {
    while (m_running) {
        std::vector<GlucoseEntry> entries;
        std::string err;
        bool ok = PerformFetch(entries, err);

        if (m_callback) {
            m_callback(entries, err);
        }

        int waitSec = 60;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            waitSec = (std::max)(15, m_config.refreshInterval);
        }

        for (int i = 0; i < waitSec * 10 && m_running; ++i) {
            if (m_forceFetch) {
                m_forceFetch = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

bool NightscoutClient::TestConnection(const std::wstring& inUrl, const std::wstring& inToken, std::string& outErr, double& outSgv) {
    std::wstring url = inUrl;
    if (url.rfind(L"http", 0) != 0) url = L"https://" + url;
    while (!url.empty() && url.back() == L'/') url.pop_back();
    if (url.find(L"/api/v1/entries") == std::wstring::npos) url += L"/api/v1/entries.json?count=1";
    if (!inToken.empty()) {
        url += (url.find(L'?') == std::wstring::npos ? L"?token=" : L"&token=") + inToken;
    }

    std::string resp;
    if (!HttpGet(url, inToken, resp, outErr)) return false;

    std::vector<GlucoseEntry> entries;
    if (!ParseEntries(resp, entries) || entries.empty()) {
        outErr = "No glucose readings found";
        return false;
    }

    outSgv = entries[0].sgv;
    return true;
}
