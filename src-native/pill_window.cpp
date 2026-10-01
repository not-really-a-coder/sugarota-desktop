#include "pill_window.h"
#include <iomanip>
#include <sstream>
#include <chrono>

#define WM_USER_PILL_CLICK (WM_USER + 101)
#define WM_USER_PILL_SETTINGS (WM_USER + 102)

#define TIMER_TOPMOST 1001

PillWindow::PillWindow(D2DContext* d2d, HWND owner, int monitorIndex, HMONITOR hMon, int widgetIndex)
    : m_d2d(d2d), m_owner(owner), m_monitorIndex(monitorIndex), m_hMon(hMon), m_widgetIndex(widgetIndex) {}

PillWindow::~PillWindow() {
    if (m_hwnd) KillTimer(m_hwnd, TIMER_TOPMOST);
    if (m_renderTarget) m_renderTarget->Release();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

RECT PillWindow::GetBounds() const {
    RECT r = { 0, 0, 0, 0 };
    if (m_hwnd) GetWindowRect(m_hwnd, &r);
    return r;
}

bool PillWindow::Create() {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = PillWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SugarotaPillWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(m_hMon, &mi);

    int pillW = 146;
    int pillH = 42;

    int offsetX = m_widgetIndex * (pillW + 8);
    int x = mi.rcWork.right - pillW - 12 - offsetX;
    int y = mi.rcWork.bottom - pillH - 6;

    if (m_config.pillX != -1 && m_config.pillY != -1 && m_monitorIndex == 0) {
        x = m_config.pillX - offsetX;
        y = m_config.pillY;
    }

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"SugarotaPillWindowClass",
        L"Sugarota Pill",
        WS_POPUP,
        x, y, pillW, pillH,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!m_hwnd) return false;

    // Clip outer edges so no square black corners show beneath rounded D2D rectangle
    HRGN hRgn = CreateRoundRectRgn(0, 0, pillW + 1, pillH + 1, 16, 16);
    SetWindowRgn(m_hwnd, hRgn, TRUE);

    // Heartbeat timer to assert topmost above Windows 11 taskbar even when clicked
    SetTimer(m_hwnd, TIMER_TOPMOST, 500, NULL);

    // Direct2D Render Target
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    HRESULT hr = m_d2d->Factory()->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        &m_renderTarget
    );

    return SUCCEEDED(hr);
}

void PillWindow::Show() {
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void PillWindow::Hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void PillWindow::UpdateData(const std::vector<GlucoseEntry>& entries, const AppConfig& config) {
    m_entries = entries;
    m_config = config;
    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void PillWindow::DrawArrow(ID2D1RenderTarget* target, ID2D1Brush* brush, const std::string& direction, float cx, float cy, float size) {
    D2DContext::DrawTrendArrow(target, brush, direction, cx, cy, size);
}

void PillWindow::OnPaint() {
    if (!m_renderTarget) return;

    m_renderTarget->BeginDraw();

    bool isDay = (m_config.theme == L"light");
    D2D1_COLOR_F bgColor = isDay ? D2D1::ColorF(0.95f, 0.95f, 0.97f, 0.96f) : D2D1::ColorF(0.12f, 0.12f, 0.13f, 0.96f);
    D2D1_COLOR_F borderColor = isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f);

    m_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    D2D1_SIZE_F size = m_renderTarget->GetSize();
    D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(D2D1::RectF(1.0f, 1.0f, size.width - 1.0f, size.height - 1.0f), 7.0f, 7.0f);

    ID2D1SolidColorBrush* bgBrush = nullptr;
    ID2D1SolidColorBrush* borderBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(bgColor, &bgBrush);
    m_renderTarget->CreateSolidColorBrush(borderColor, &borderBrush);

    if (bgBrush && borderBrush) {
        m_renderTarget->FillRoundedRectangle(rrect, bgBrush);
        m_renderTarget->DrawRoundedRectangle(rrect, borderBrush, 1.0f);
        bgBrush->Release();
        borderBrush->Release();
    }

    double currentSgv = 0.0;
    std::string direction = "Flat";
    double delta = 0.0;
    long long lastDate = 0;

    if (!m_entries.empty()) {
        currentSgv = m_entries[0].sgv;
        direction = m_entries[0].direction;
        delta = m_entries[0].delta;
        lastDate = m_entries[0].date;
    }

    D2D1_COLOR_F sgvColor = D2DContext::GetGlucoseColor(currentSgv > 0 ? currentSgv : 100.0, isDay, m_config.alarmLow, m_config.alarmHigh);
    ID2D1SolidColorBrush* colorBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(sgvColor, &colorBrush);

    // DirectWrite Text Formatting
    IDWriteTextFormat* heroFormat = nullptr;
    IDWriteTextFormat* smallFormat = nullptr;
    IDWriteTextFormat* ageFormat = nullptr;

    // Use Black weight as in flyout window hero
    m_d2d->DWriteFactory()->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 16.0f, L"en-us", &heroFormat
    );

    m_d2d->DWriteFactory()->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &smallFormat
    );

    m_d2d->DWriteFactory()->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 9.0f, L"en-us", &ageFormat
    );
    if (ageFormat) {
        ageFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    // Calculate time difference in minutes
    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    long long diffMins = (lastDate > 0 && nowMs >= lastDate) ? ((nowMs - lastDate) / 60000LL) : 0;
    if (diffMins < 0) diffMins = 0;

    // Left: Harvey Circle
    // Previously: harveyX = 14.0f, harveyY = 13.0f, harveyR = 4.5f (diameter = 9px, bounds x: 9.5..18.5, y: 8.5..17.5).
    // User requested: Increase diameter by 1px by expanding down and to the right:
    // New diameter = 10px (radius = 5.0f).
    // Left stays at 9.5 -> harveyX = 9.5 + 5.0 = 14.5f.
    // Top stays at 8.5 -> harveyY = 8.5 + 5.0 = 13.5f.
    float harveyX = 14.5f;
    float harveyY = 13.5f;
    float harveyR = 5.0f;

    D2D1_COLOR_F harveyColor = isDay ? D2D1::ColorF(0.0f, 0.6f, 0.2f) : D2D1::ColorF(0.0f, 1.0f, 0.27f);
    if (diffMins >= 15) harveyColor = isDay ? D2D1::ColorF(0.8f, 0.0f, 0.0f) : D2D1::ColorF(1.0f, 0.27f, 0.27f);
    else if (diffMins >= 6) harveyColor = isDay ? D2D1::ColorF(0.85f, 0.47f, 0.0f) : D2D1::ColorF(1.0f, 0.67f, 0.0f);

    ID2D1SolidColorBrush* harveyBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(harveyColor, &harveyBrush);
    if (harveyBrush) {
        D2D1_ELLIPSE hEllipse = D2D1::Ellipse(D2D1::Point2F(harveyX, harveyY), harveyR, harveyR);
        m_renderTarget->DrawEllipse(hEllipse, harveyBrush, 1.5f);

        // Fill quadrant pie based on age (0-5+ mins)
        float fillPct = 0.0f;
        if (diffMins >= 5) fillPct = 1.0f;
        else if (diffMins > 0) fillPct = (float)diffMins * 0.2f;

        if (fillPct >= 0.99f) {
            m_renderTarget->FillEllipse(hEllipse, harveyBrush);
        } else if (fillPct > 0.0f) {
            ID2D1PathGeometry* pieGeo = nullptr;
            m_d2d->Factory()->CreatePathGeometry(&pieGeo);
            if (pieGeo) {
                ID2D1GeometrySink* sink = nullptr;
                if (SUCCEEDED(pieGeo->Open(&sink))) {
                    sink->BeginFigure(D2D1::Point2F(harveyX, harveyY), D2D1_FIGURE_BEGIN_FILLED);
                    float startAngle = -90.0f;
                    float endAngle = startAngle + fillPct * 360.0f;
                    float radStart = startAngle * 3.14159265f / 180.0f;
                    float radEnd = endAngle * 3.14159265f / 180.0f;
                    sink->AddLine(D2D1::Point2F(harveyX + cosf(radStart) * harveyR, harveyY + sinf(radStart) * harveyR));
                    sink->AddArc(D2D1::ArcSegment(
                        D2D1::Point2F(harveyX + cosf(radEnd) * harveyR, harveyY + sinf(radEnd) * harveyR),
                        D2D1::SizeF(harveyR, harveyR), 0.0f,
                        fillPct > 0.5f ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE,
                        fillPct > 0.5f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL
                    ));
                    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    sink->Close();
                    sink->Release();
                    m_renderTarget->FillGeometry(pieGeo, harveyBrush);
                }
                pieGeo->Release();
            }
        }
        harveyBrush->Release();
    }

    // Sub-label brush (muted for delta/units/age)
    ID2D1SolidColorBrush* subBrush = nullptr;
    D2D1_COLOR_F subColor = isDay ? D2D1::ColorF(0.45f, 0.45f, 0.45f) : D2D1::ColorF(0.70f, 0.70f, 0.70f);
    m_renderTarget->CreateSolidColorBrush(subColor, &subBrush);

    // Data age text below the Harvey ball:
    // User requested: move 1px lower (y = 24.0f) and 1px to the right (x = 1.0f .. 29.0f)
    if (subBrush && ageFormat) {
        std::wstring ageStr = (diffMins <= 0) ? L"now" : (std::to_wstring(diffMins) + L"m");
        m_renderTarget->DrawText(
            ageStr.c_str(), (UINT32)ageStr.length(), ageFormat,
            D2D1::RectF(1.0f, 24.0f, 29.0f, size.height), subBrush
        );
    }

    // Glucose Value Number
    std::wstringstream valStream;
    bool isMmol = (m_config.unit == L"mmol");
    if (currentSgv > 0) {
        if (isMmol) valStream << std::fixed << std::setprecision(1) << (currentSgv / 18.0182);
        else valStream << (int)currentSgv;
    } else {
        valStream << L"--";
    }
    std::wstring valStr = valStream.str();

    // Color-coded brush for glucose value & trend arrow
    ID2D1SolidColorBrush* heroBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(sgvColor, &heroBrush);

    float textLeft = 28.0f;
    float textRight = size.width - 6.0f;

    // Measure Value number width using IDWriteTextLayout so the arrow can be placed right next to it
    float valWidth = 28.0f;
    IDWriteTextLayout* valLayout = nullptr;
    if (SUCCEEDED(m_d2d->DWriteFactory()->CreateTextLayout(
        valStr.c_str(), (UINT32)valStr.length(), heroFormat,
        150.0f, 30.0f, &valLayout))) {
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(valLayout->GetMetrics(&tm))) {
            valWidth = tm.widthIncludingTrailingWhitespace;
        }
    }

    // Draw Value number
    if (heroBrush) {
        if (valLayout) {
            m_renderTarget->DrawTextLayout(D2D1::Point2F(textLeft, 2.0f), valLayout, heroBrush);
            valLayout->Release();
        } else if (heroFormat) {
            m_renderTarget->DrawText(
                valStr.c_str(), (UINT32)valStr.length(), heroFormat,
                D2D1::RectF(textLeft, 2.0f, textRight, 24.0f), heroBrush
            );
        }
    }

    // Trend Arrow directly to the right of glucose value:
    // Left edge of the arrow container is placed at `textLeft + valWidth + 12.0f`.
    // For DoubleUp/DoubleDown, center is offset by +3.2f so its leftmost shaft aligns with single arrow.
    float arrowLeftMargin = textLeft + valWidth + 12.0f;
    bool isDouble = (direction == "DoubleUp" || direction == "DoubleDown");
    float arrowCx = isDouble ? (arrowLeftMargin + 3.2f) : arrowLeftMargin;
    float arrowCy = 14.0f;

    if (heroBrush) {
        DrawArrow(m_renderTarget, heroBrush, direction, arrowCx, arrowCy, 11.5f);
        heroBrush->Release();
    }

    // Delta & Units on second line (without triangle symbol) - at y=23.0f
    std::wstringstream deltaStream;
    if (delta != 0.0) {
        double dVal = isMmol ? (delta / 18.0182) : delta;
        if (delta > 0) deltaStream << L"+";
        if (isMmol) deltaStream << std::fixed << std::setprecision(1) << dVal;
        else deltaStream << (int)dVal;
    } else {
        deltaStream << L"0";
    }
    deltaStream << (isMmol ? L" mmol/L" : L" mg/dL");

    std::wstring deltaStr = deltaStream.str();
    if (subBrush && smallFormat) {
        m_renderTarget->DrawText(
            deltaStr.c_str(), (UINT32)deltaStr.length(), smallFormat,
            D2D1::RectF(textLeft, 23.0f, textRight, size.height), subBrush
        );
    }

    // Mini Chart Preview in the right empty area of the pill
    // User requested: decrease width by 10px from the left side
    float arrowRightEdge = arrowCx + (isDouble ? 3.2f : 0.0f) + 2.0f;
    float miniMargin = 7.0f;
    float miniLeft = arrowRightEdge + miniMargin + 13.0f;
    float miniRight = size.width - miniMargin;
    float miniTop = 7.0f;
    float miniBottom = size.height - 7.0f;
    float miniW = miniRight - miniLeft;
    float miniH = miniBottom - miniTop;

    // Filter mini-chart entries to ONLY the last 1 hour (3600000 ms)
    std::vector<GlucoseEntry> miniPts;
    if (!m_entries.empty() && miniW > 10.0f) {
        long long latestTime = m_entries[0].date;
        long long oneHourCutoff = latestTime - (60LL * 60LL * 1000LL);
        for (const auto& e : m_entries) {
            if (e.date >= oneHourCutoff) {
                miniPts.push_back(e);
            }
        }
        // Fallback: if fewer than 2 points in 1h, take up to 12 most recent points (1 hour of 5-min readings)
        if (miniPts.size() < 2) {
            size_t maxCount = (std::min)((size_t)12, m_entries.size());
            miniPts.assign(m_entries.begin(), m_entries.begin() + maxCount);
        }
    }

    if (miniPts.size() >= 2) {
        std::reverse(miniPts.begin(), miniPts.end());

        double minSgv = 60.0;
        double maxSgv = 200.0;
        for (const auto& p : miniPts) {
            if (p.sgv < minSgv) minSgv = p.sgv;
            if (p.sgv > maxSgv) maxSgv = p.sgv;
        }
        minSgv = (std::min)(minSgv, m_config.alarmLow - 10.0);
        maxSgv = (std::max)(maxSgv, m_config.alarmHigh + 10.0);
        double range = (maxSgv > minSgv) ? (maxSgv - minSgv) : 50.0;

        auto getMiniY = [&](double sgv) -> float {
            float norm = (float)((sgv - minSgv) / range);
            return miniBottom - (norm * miniH);
        };

        // Target range background corridor
        float yHigh = getMiniY(m_config.alarmHigh);
        float yLow = getMiniY(m_config.alarmLow);

        ID2D1SolidColorBrush* miniCorridorBrush = nullptr;
        m_renderTarget->CreateSolidColorBrush(
            isDay ? D2D1::ColorF(0.0f, 0.60f, 0.20f, 0.12f) : D2D1::ColorF(0.0f, 1.0f, 0.26f, 0.10f),
            &miniCorridorBrush
        );
        if (miniCorridorBrush) {
            m_renderTarget->FillRectangle(
                D2D1::RectF(miniLeft, (std::min)(yHigh, yLow), miniRight, (std::max)(yHigh, yLow)),
                miniCorridorBrush
            );
            miniCorridorBrush->Release();
        }

        // Color-coded line with proportional time scaling over the 1-hour window
        D2D1_COLOR_F greenColor = isDay ? D2D1::ColorF(0.0f, 0.60f, 0.20f) : D2D1::ColorF(0.0f, 1.0f, 0.26f);
        D2D1_COLOR_F orangeColor = isDay ? D2D1::ColorF(0.85f, 0.46f, 0.0f) : D2D1::ColorF(1.0f, 0.60f, 0.0f);

        ID2D1SolidColorBrush* greenBrush = nullptr;
        ID2D1SolidColorBrush* orangeBrush = nullptr;
        m_renderTarget->CreateSolidColorBrush(greenColor, &greenBrush);
        m_renderTarget->CreateSolidColorBrush(orangeColor, &orangeBrush);

        long long miniEndTime = miniPts.back().date;
        long long miniStartTime = miniEndTime - (60LL * 60LL * 1000LL); // 1 hour window
        long long miniDuration = miniEndTime - miniStartTime;
        if (miniDuration <= 0) miniDuration = 1;

        auto getMiniX = [&](long long dateMs) -> float {
            if (dateMs <= miniStartTime) return miniLeft;
            if (dateMs >= miniEndTime) return miniRight;
            float norm = (float)(dateMs - miniStartTime) / (float)miniDuration;
            return miniLeft + (norm * miniW);
        };

        const long long maxMiniConnectGapMs = 6LL * 60LL * 1000LL;

        for (size_t i = 0; i + 1 < miniPts.size(); ++i) {
            long long gap = miniPts[i + 1].date - miniPts[i].date;
            if (gap > maxMiniConnectGapMs) {
                continue; // Skip connecting points if gap > 6 min
            }

            float x1 = getMiniX(miniPts[i].date);
            float y1 = getMiniY(miniPts[i].sgv);
            float x2 = getMiniX(miniPts[i + 1].date);
            float y2 = getMiniY(miniPts[i + 1].sgv);

            bool bothInside = (miniPts[i].sgv >= m_config.alarmLow && miniPts[i].sgv <= m_config.alarmHigh) &&
                              (miniPts[i + 1].sgv >= m_config.alarmLow && miniPts[i + 1].sgv <= m_config.alarmHigh);
            ID2D1SolidColorBrush* segBrush = bothInside ? greenBrush : orangeBrush;
            if (segBrush) {
                m_renderTarget->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), segBrush, 1.5f);
            }
        }

        if (greenBrush) greenBrush->Release();
        if (orangeBrush) orangeBrush->Release();
    }

    if (subBrush) subBrush->Release();
    if (heroFormat) heroFormat->Release();
    if (smallFormat) smallFormat->Release();
    if (ageFormat) ageFormat->Release();

    m_renderTarget->EndDraw();
}

LRESULT CALLBACK PillWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PillWindow* pThis = (PillWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        pThis = (PillWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (pThis) pThis->OnPaint();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        pThis->m_isDragging = true;
        GetCursorPos(&pThis->m_dragStartMouse);
        RECT r;
        GetWindowRect(hwnd, &r);
        pThis->m_dragStartWindow.x = r.left;
        pThis->m_dragStartWindow.y = r.top;
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (pThis && pThis->m_isDragging) {
            POINT pt;
            GetCursorPos(&pt);
            int dx = pt.x - pThis->m_dragStartMouse.x;
            int dy = pt.y - pThis->m_dragStartMouse.y;
            SetWindowPos(hwnd, HWND_TOPMOST,
                         pThis->m_dragStartWindow.x + dx,
                         pThis->m_dragStartWindow.y + dy,
                         0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (pThis && pThis->m_isDragging) {
            ReleaseCapture();
            pThis->m_isDragging = false;

            POINT pt;
            GetCursorPos(&pt);
            int moved = abs(pt.x - pThis->m_dragStartMouse.x) + abs(pt.y - pThis->m_dragStartMouse.y);

            if (moved < 5) {
                // Click -> Open Flyout
                if (pThis->m_owner) {
                    PostMessageW(pThis->m_owner, WM_USER_PILL_CLICK, (WPARAM)pThis->m_monitorIndex, 0);
                }
            } else {
                // Dragged -> Save Position
                RECT r;
                GetWindowRect(hwnd, &r);
                pThis->m_config.pillX = r.left;
                pThis->m_config.pillY = r.top;
                ConfigManager::Save(pThis->m_config);
            }
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        // Right Click -> Open Settings
        if (pThis && pThis->m_owner) {
            PostMessageW(pThis->m_owner, WM_USER_PILL_SETTINGS, 0, 0);
        }
        return 0;
    }
    case WM_TIMER: {
        if (wParam == TIMER_TOPMOST) {
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    }
    case WM_ACTIVATE:
    case WM_KILLFOCUS: {
        // Enforce topmost over the taskbar
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
