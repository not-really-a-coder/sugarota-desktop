#pragma once
#include "config.h"
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>

class NightscoutClient {
public:
    using Callback = std::function<void(const std::vector<GlucoseEntry>&, const std::string& error)>;

    NightscoutClient();
    ~NightscoutClient();

    void Start(const AppConfig& config, Callback callback);
    void Stop();
    void FetchNow();
    void UpdateConfig(const AppConfig& config);

    static bool TestConnection(const std::wstring& url, const std::wstring& token, std::string& outErr, double& outSgv);

private:
    void WorkerLoop();
    bool PerformFetch(std::vector<GlucoseEntry>& entries, std::string& error);

    AppConfig m_config;
    Callback m_callback;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_forceFetch{false};
    std::mutex m_mutex;
};
