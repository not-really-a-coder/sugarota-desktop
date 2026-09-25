#include <windows.h>
#include <shellapi.h>
#include <vector>
#include <memory>
#include <iostream>
#include "config.h"
#include "d2d_context.h"
#include "pill_window.h"
#include "flyout_window.h"
#include "settings_dialog.h"
#include "nightscout.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

#define WM_TRAYICON (WM_USER + 100)
#define ID_TRAY_SETTINGS 1001
#define ID_TRAY_REFRESH 1002
#define ID_TRAY_RESET 1003
#define ID_TRAY_EXIT 1004

class AppManager {
public:
    AppManager() {}
    ~AppManager() {
        RemoveTrayIcon();
    }

    bool Init(HINSTANCE hInst) {
        m_hInst = hInst;
        ConfigManager::Load(m_config);

        if (!m_d2d.Init()) {
            MessageBoxW(NULL, L"Failed to initialize Direct2D", L"Error", MB_ICONERROR);
            return false;
        }

        // Hidden Manager Window for handling tray and messages
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = AppManager::MainWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"SugarotaManagerClass";
        RegisterClassExW(&wc);

        m_hwnd = CreateWindowExW(0, L"SugarotaManagerClass", L"Sugarota Manager", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInst, this);
        if (!m_hwnd) return false;

        m_flyout = std::make_unique<FlyoutWindow>(&m_d2d, [this]() {
            OpenSettings();
        });
        m_flyout->Create();

        SetupTrayIcon();
        SyncMonitors();

        // Start Nightscout background client
        m_client.Start(m_config, [this](const std::vector<GlucoseEntry>& entries, const std::string& err) {
            PostMessageW(m_hwnd, WM_USER + 201, (WPARAM)new std::vector<GlucoseEntry>(entries), 0);
        });

        // Show settings if first run
        if (m_config.nightscoutUrl.empty()) {
            OpenSettings();
        }

        return true;
    }

    void Run() {
        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void SyncMonitors() {
        m_pills.clear();

        struct MonitorEnumData {
            std::vector<HMONITOR> monitors;
        } data;

        EnumDisplayMonitors(NULL, NULL, [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL {
            auto* d = (MonitorEnumData*)lp;
            d->monitors.push_back(hMon);
            return TRUE;
        }, (LPARAM)&data);

        int monitorCount = m_config.showOnAllMonitors ? (int)data.monitors.size() : 1;
        int widgetTotal = (std::max)(1, (std::min)(5, m_config.widgetCount));

        for (int m = 0; m < monitorCount; ++m) {
            for (int w = 0; w < widgetTotal; ++w) {
                auto pill = std::make_unique<PillWindow>(&m_d2d, m_hwnd, m, data.monitors[m], w);
                if (pill->Create()) {
                    pill->UpdateData(m_latestEntries, m_config);
                    pill->Show();
                    m_pills.push_back(std::move(pill));
                }
            }
        }
    }

    void OpenSettings() {
        if (!m_settings) {
            m_settings = std::make_unique<SettingsDialog>(m_hwnd, m_config, [this](const AppConfig& updated) {
                m_config = updated;
                m_client.UpdateConfig(m_config);
                SyncMonitors();
                UpdateAllWindows();
            });
        }
        m_settings->Show();
    }

    void UpdateAllWindows() {
        for (auto& p : m_pills) {
            p->UpdateData(m_latestEntries, m_config);
        }
        if (m_flyout) {
            m_flyout->UpdateData(m_latestEntries, m_config);
        }
    }

    void ToggleFlyout(int monitorIndex) {
        if (m_flyout->IsVisible()) {
            m_flyout->Hide();
        } else {
            RECT anchor = { 0, 0, 0, 0 };
            if (monitorIndex < (int)m_pills.size()) {
                anchor = m_pills[monitorIndex]->GetBounds();
            } else if (!m_pills.empty()) {
                anchor = m_pills[0]->GetBounds();
            }
            m_flyout->UpdateData(m_latestEntries, m_config);
            m_flyout->ShowNear(anchor);
        }
    }

private:
    void SetupTrayIcon() {
        m_nid.cbSize = sizeof(m_nid);
        m_nid.hWnd = m_hwnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;
        m_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wcscpy_s(m_nid.szTip, L"Sugarota Desktop (Native)");
        Shell_NotifyIconW(NIM_ADD, &m_nid);
    }

    void RemoveTrayIcon() {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
    }

    void ShowTrayMenu() {
        POINT pt;
        GetCursorPos(&pt);

        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING | MF_GRAYED, 0, L"Sugarota Desktop");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Open Settings");
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_REFRESH, L"Refresh Data Now");
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_RESET, L"Reset Pill Position");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

        SetForegroundWindow(m_hwnd);
        TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
        DestroyMenu(hMenu);
    }

    static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        AppManager* pThis = (AppManager*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            pThis = (AppManager*)cs->lpCreateParams;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                if (pThis) pThis->ShowTrayMenu();
            } else if (lParam == WM_LBUTTONUP) {
                if (pThis) pThis->OpenSettings();
            }
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_TRAY_SETTINGS && pThis) pThis->OpenSettings();
            else if (id == ID_TRAY_REFRESH && pThis) pThis->m_client.FetchNow();
            else if (id == ID_TRAY_RESET && pThis) {
                pThis->m_config.pillX = -1;
                pThis->m_config.pillY = -1;
                ConfigManager::Save(pThis->m_config);
                pThis->SyncMonitors();
            } else if (id == ID_TRAY_EXIT) {
                PostQuitMessage(0);
            }
            return 0;
        }
        case (WM_USER + 101): { // Pill clicked
            if (pThis) pThis->ToggleFlyout((int)wParam);
            return 0;
        }
        case (WM_USER + 102): { // Pill right-clicked
            if (pThis) pThis->OpenSettings();
            return 0;
        }
        case (WM_USER + 201): { // Glucose update received
            auto* pEntries = (std::vector<GlucoseEntry>*)wParam;
            if (pThis && pEntries) {
                pThis->m_latestEntries = *pEntries;
                delete pEntries;
                pThis->UpdateAllWindows();
            }
            return 0;
        }
        case WM_DISPLAYCHANGE: {
            if (pThis) pThis->SyncMonitors();
            return 0;
        }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    HINSTANCE m_hInst = NULL;
    HWND m_hwnd = NULL;
    NOTIFYICONDATAW m_nid = { 0 };

    D2DContext m_d2d;
    AppConfig m_config;
    NightscoutClient m_client;
    std::vector<GlucoseEntry> m_latestEntries;

    std::vector<std::unique_ptr<PillWindow>> m_pills;
    std::unique_ptr<FlyoutWindow> m_flyout;
    std::unique_ptr<SettingsDialog> m_settings;
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // Single instance mutex
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"SugarotaDesktopNativeMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"Sugarota Desktop is already running. Check your system tray.", L"Sugarota Desktop", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    AppManager app;
    if (!app.Init(hInstance)) {
        return 1;
    }

    app.Run();

    CloseHandle(hMutex);
    return 0;
}
