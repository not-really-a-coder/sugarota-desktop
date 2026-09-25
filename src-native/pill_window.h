#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <memory>
#include "d2d_context.h"
#include "config.h"

class PillWindow {
public:
    PillWindow(D2DContext* d2d, HWND owner, int monitorIndex, HMONITOR hMon, int widgetIndex = 0);
    ~PillWindow();

    bool Create();
    void Show();
    void Hide();
    void UpdateData(const std::vector<GlucoseEntry>& entries, const AppConfig& config);
    HWND Hwnd() const { return m_hwnd; }
    RECT GetBounds() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void OnResize(UINT width, UINT height);
    void DrawArrow(ID2D1RenderTarget* target, ID2D1Brush* brush, const std::string& direction, float cx, float cy, float size);

    D2DContext* m_d2d = nullptr;
    HWND m_owner = NULL;
    HWND m_hwnd = NULL;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;
    int m_monitorIndex = 0;
    int m_widgetIndex = 0;
    HMONITOR m_hMon = NULL;

    std::vector<GlucoseEntry> m_entries;
    AppConfig m_config;

    bool m_isDragging = false;
    POINT m_dragStartMouse = { 0, 0 };
    POINT m_dragStartWindow = { 0, 0 };
};
