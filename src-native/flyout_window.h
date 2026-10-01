#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <vector>
#include <functional>
#include "d2d_context.h"
#include "config.h"

class FlyoutWindow {
public:
    FlyoutWindow(D2DContext* d2d, std::function<void()> onSettingsClick = nullptr, std::function<void()> onRefreshClick = nullptr);
    ~FlyoutWindow();

    bool Create();
    void ShowNear(const RECT& anchorRect);
    void Hide();
    bool IsVisible() const;
    void UpdateData(const std::vector<GlucoseEntry>& entries, const AppConfig& config);
    void SetRefreshing(bool refreshing);
    void TriggerRefreshAnimation();
    HWND Hwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint();
    void DrawChart(ID2D1RenderTarget* target, const D2D1_RECT_F& rect, bool isDay);

    D2DContext* m_d2d = nullptr;
    HWND m_hwnd = NULL;
    ID2D1HwndRenderTarget* m_renderTarget = nullptr;

    std::vector<GlucoseEntry> m_entries;
    AppConfig m_config;

    // Scrubber hover state
    bool m_hoveringChart = false;
    float m_mouseChartX = 0.0f;
    bool m_trackingMouse = false;

    // Pin state (always on top, does not hide on deactivation)
    bool m_isPinned = false;

    // Refresh state & spin animation
    bool m_isRefreshing = false;
    float m_refreshAngle = 0.0f;
    bool m_animatingRefresh = false;
    ULONGLONG m_lastAnimTick = 0;
    ULONGLONG m_animStartTick = 0;

    std::function<void()> m_onSettingsClick;
    std::function<void()> m_onRefreshClick;
};
