#include "settings_dialog.h"
#include "nightscout.h"
#include <commctrl.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

#define ID_BTN_TEST 201
#define ID_BTN_SAVE 202
#define ID_BTN_CANCEL 203
#define ID_BTN_EXIT 204

SettingsDialog::SettingsDialog(HWND parent, const AppConfig& config, OnSaveCallback onSave)
    : m_parent(parent), m_config(config), m_onSave(onSave) {}

SettingsDialog::~SettingsDialog() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void SettingsDialog::Show() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_RESTORE);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(m_hwnd);
        return;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SettingsDialog::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SugarotaSettingsDialogClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    int clientW = 460;
    int clientH = 486;

    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_TOPMOST | WS_EX_APPWINDOW);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        L"SugarotaSettingsDialogClass",
        L"Sugarota Desktop - Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!m_hwnd) return;

    CreateControls();
    ShowWindow(m_hwnd, SW_SHOW);
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(m_hwnd);
}

void SettingsDialog::CreateControls() {
    HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HFONT hFontBold = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    auto addLabel = [&](const wchar_t* text, int x, int y, int w, int h, bool bold = false) {
        HWND lbl = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
        SendMessageW(lbl, WM_SETFONT, (WPARAM)(bold ? hFontBold : hFont), TRUE);
        return lbl;
    };

    // Nightscout Connection Group
    HWND grpConn = CreateWindowW(L"BUTTON", L" Nightscout API Connection ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 12, 420, 164, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(grpConn, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    addLabel(L"Nightscout URL:", 32, 38, 380, 18);
    m_editUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_config.nightscoutUrl.c_str(),
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 32, 58, 388, 26,
                                m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_editUrl, WM_SETFONT, (WPARAM)hFont, TRUE);

    addLabel(L"API Secret / Token (optional if public):", 32, 90, 380, 18);
    m_editSecret = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_config.apiSecret.c_str(),
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD, 32, 110, 388, 26,
                                   m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_editSecret, WM_SETFONT, (WPARAM)hFont, TRUE);

    HWND btnTest = CreateWindowW(L"BUTTON", L"Test Connection", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 32, 142, 130, 26, m_hwnd, (HMENU)ID_BTN_TEST, GetModuleHandle(NULL), NULL);
    SendMessageW(btnTest, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_lblStatus = addLabel(L"", 170, 146, 250, 20);

    // Appearance Group
    HWND grpApp = CreateWindowW(L"BUTTON", L" Appearance & Units ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 186, 420, 106, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(grpApp, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    addLabel(L"Units:", 32, 212, 80, 20);
    m_radioMmol = CreateWindowW(L"BUTTON", L"mmol/L", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                                110, 212, 90, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    m_radioMgdl = CreateWindowW(L"BUTTON", L"mg/dL", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                210, 212, 90, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_radioMmol, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_radioMgdl, WM_SETFONT, (WPARAM)hFont, TRUE);

    if (m_config.unit == L"mgdl") SendMessageW(m_radioMgdl, BM_SETCHECK, BST_CHECKED, 0);
    else SendMessageW(m_radioMmol, BM_SETCHECK, BST_CHECKED, 0);

    addLabel(L"Theme:", 32, 248, 80, 20);
    m_radioDark = CreateWindowW(L"BUTTON", L"Dark Mode", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                                110, 248, 100, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    m_radioLight = CreateWindowW(L"BUTTON", L"Light Mode", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                 220, 248, 100, 20, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_radioDark, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_radioLight, WM_SETFONT, (WPARAM)hFont, TRUE);

    if (m_config.theme == L"light") SendMessageW(m_radioLight, BM_SETCHECK, BST_CHECKED, 0);
    else SendMessageW(m_radioDark, BM_SETCHECK, BST_CHECKED, 0);

    // Multi-Monitor & Widget Count Group
    HWND grpWidgets = CreateWindowW(L"BUTTON", L" Widgets & Displays ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 16, 302, 420, 116, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(grpWidgets, WM_SETFONT, (WPARAM)hFontBold, TRUE);

    addLabel(L"Widgets on primary monitor (1 to 5):", 32, 328, 250, 20);
    m_comboWidgets = CreateWindowExW(0, L"COMBOBOX", L"",
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                    286, 324, 70, 150, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_comboWidgets, WM_SETFONT, (WPARAM)hFont, TRUE);

    for (int i = 1; i <= 5; ++i) {
        std::wstring num = std::to_wstring(i);
        SendMessageW(m_comboWidgets, CB_ADDSTRING, 0, (LPARAM)num.c_str());
    }
    int selIdx = (std::max)(0, (std::min)(4, m_config.widgetCount - 1));
    SendMessageW(m_comboWidgets, CB_SETCURSEL, selIdx, 0);

    m_chkAllMonitors = CreateWindowW(L"BUTTON", L"Repeat widgets on secondary monitors",
                                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                     32, 356, 380, 22, m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_chkAllMonitors, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(m_chkAllMonitors, BM_SETCHECK, m_config.showOnAllMonitors ? BST_CHECKED : BST_UNCHECKED, 0);

    addLabel(L"Refresh interval (seconds):", 32, 384, 200, 20);
    m_editInterval = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", std::to_wstring(m_config.refreshInterval).c_str(),
                                     WS_CHILD | WS_VISIBLE | ES_NUMBER, 236, 382, 50, 24,
                                     m_hwnd, NULL, GetModuleHandle(NULL), NULL);
    SendMessageW(m_editInterval, WM_SETFONT, (WPARAM)hFont, TRUE);

    // Save, Cancel, and Exit Buttons
    HWND btnExit = CreateWindowW(L"BUTTON", L"Exit Sugarota", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 16, 432, 110, 32, m_hwnd, (HMENU)ID_BTN_EXIT, GetModuleHandle(NULL), NULL);
    HWND btnSave = CreateWindowW(L"BUTTON", L"Save & Apply", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                 228, 432, 114, 32, m_hwnd, (HMENU)ID_BTN_SAVE, GetModuleHandle(NULL), NULL);
    HWND btnCancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                   350, 432, 86, 32, m_hwnd, (HMENU)ID_BTN_CANCEL, GetModuleHandle(NULL), NULL);
    SendMessageW(btnExit, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessageW(btnSave, WM_SETFONT, (WPARAM)hFontBold, TRUE);
    SendMessageW(btnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
}

void SettingsDialog::OnTestConnection() {
    wchar_t urlBuf[512] = { 0 };
    wchar_t secBuf[512] = { 0 };
    GetWindowTextW(m_editUrl, urlBuf, 512);
    GetWindowTextW(m_editSecret, secBuf, 512);

    SetWindowTextW(m_lblStatus, L"Connecting...");

    std::string err;
    double sgv = 0.0;
    bool ok = NightscoutClient::TestConnection(urlBuf, secBuf, err, sgv);

    if (ok) {
        std::wstringstream ss;
        ss << L"Connected! SGV: " << (int)sgv << L" mg/dL";
        SetWindowTextW(m_lblStatus, ss.str().c_str());
    } else {
        std::wstring werr(err.begin(), err.end());
        SetWindowTextW(m_lblStatus, (L"Failed: " + werr).c_str());
    }
}

void SettingsDialog::OnSave() {
    wchar_t urlBuf[512] = { 0 };
    wchar_t secBuf[512] = { 0 };
    wchar_t intBuf[64] = { 0 };

    GetWindowTextW(m_editUrl, urlBuf, 512);
    GetWindowTextW(m_editSecret, secBuf, 512);
    GetWindowTextW(m_editInterval, intBuf, 64);

    m_config.nightscoutUrl = urlBuf;
    m_config.apiSecret = secBuf;
    m_config.refreshInterval = _wtoi(intBuf);
    if (m_config.refreshInterval < 15) m_config.refreshInterval = 15;

    m_config.unit = (SendMessageW(m_radioMgdl, BM_GETCHECK, 0, 0) == BST_CHECKED) ? L"mgdl" : L"mmol";
    m_config.theme = (SendMessageW(m_radioLight, BM_GETCHECK, 0, 0) == BST_CHECKED) ? L"light" : L"dark";
    m_config.showOnAllMonitors = (SendMessageW(m_chkAllMonitors, BM_GETCHECK, 0, 0) == BST_CHECKED);

    int curSel = (int)SendMessageW(m_comboWidgets, CB_GETCURSEL, 0, 0);
    m_config.widgetCount = (curSel >= 0) ? (curSel + 1) : 1;

    ConfigManager::Save(m_config);

    if (m_onSave) {
        m_onSave(m_config);
    }

    DestroyWindow(m_hwnd);
    m_hwnd = NULL;
}

LRESULT CALLBACK SettingsDialog::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* pThis = (SettingsDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        pThis = (SettingsDialog*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == ID_BTN_TEST && pThis) pThis->OnTestConnection();
        else if (id == ID_BTN_SAVE && pThis) pThis->OnSave();
        else if (id == ID_BTN_CANCEL && pThis) {
            DestroyWindow(hwnd);
            pThis->m_hwnd = NULL;
        } else if (id == ID_BTN_EXIT) {
            if (MessageBoxW(hwnd, L"Are you sure you want to exit Sugarota Desktop?", L"Exit Sugarota", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                PostQuitMessage(0);
            }
        }
        return 0;
    }
    case WM_CLOSE: {
        DestroyWindow(hwnd);
        if (pThis) pThis->m_hwnd = NULL;
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
