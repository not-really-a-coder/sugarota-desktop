#pragma once
#include <windows.h>
#include <functional>
#include "config.h"

class SettingsDialog {
public:
    using OnSaveCallback = std::function<void(const AppConfig&)>;

    SettingsDialog(HWND parent, const AppConfig& config, OnSaveCallback onSave);
    ~SettingsDialog();

    void Show();
    HWND Hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void OnTestConnection();
    void OnSave();

    HWND m_parent = NULL;
    HWND m_hwnd = NULL;
    AppConfig m_config;
    OnSaveCallback m_onSave;

    HWND m_editUrl = NULL;
    HWND m_editSecret = NULL;
    HWND m_radioMmol = NULL;
    HWND m_radioMgdl = NULL;
    HWND m_radioDark = NULL;
    HWND m_radioLight = NULL;
    HWND m_editInterval = NULL;
    HWND m_comboWidgets = NULL;
    HWND m_chkAllMonitors = NULL;
    HWND m_lblStatus = NULL;
};
