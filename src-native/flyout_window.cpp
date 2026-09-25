#include "flyout_window.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

FlyoutWindow::FlyoutWindow(D2DContext* d2d, std::function<void()> onSettingsClick)
    : m_d2d(d2d), m_onSettingsClick(onSettingsClick) {}

FlyoutWindow::~FlyoutWindow() {
    if (m_renderTarget) m_renderTarget->Release();
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool FlyoutWindow::IsVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

bool FlyoutWindow::Create() {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = FlyoutWindow::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"SugarotaFlyoutWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int flyoutW = 320;
    int flyoutH = 300;

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        L"SugarotaFlyoutWindowClass",
        L"Sugarota Flyout",
        WS_POPUP,
        0, 0, flyoutW, flyoutH,
        NULL, NULL, GetModuleHandle(NULL), this
    );

    if (!m_hwnd) return false;

    // Clip outer edges so no square black corners show beneath rounded D2D rectangle
    HRGN hRgn = CreateRoundRectRgn(0, 0, flyoutW + 1, flyoutH + 1, 20, 20);
    SetWindowRgn(m_hwnd, hRgn, TRUE);

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

void FlyoutWindow::ShowNear(const RECT& anchor) {
    if (!m_hwnd) return;

    int flyoutW = 320;
    int flyoutH = 300;

    int x = (anchor.left + anchor.right) / 2 - (flyoutW / 2);
    int y = anchor.top - flyoutH - 8;

    HMONITOR hMon = MonitorFromRect(&anchor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hMon, &mi);

    if (x < mi.rcWork.left + 8) x = mi.rcWork.left + 8;
    if (x + flyoutW > mi.rcWork.right - 8) x = mi.rcWork.right - flyoutW - 8;
    if (y < mi.rcWork.top + 8) y = anchor.bottom + 8;

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, flyoutW, flyoutH, SWP_SHOWWINDOW);
    ShowWindow(m_hwnd, SW_SHOW);
    InvalidateRect(m_hwnd, NULL, FALSE);
    SetForegroundWindow(m_hwnd);
}

void FlyoutWindow::Hide() {
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void FlyoutWindow::UpdateData(const std::vector<GlucoseEntry>& entries, const AppConfig& config) {
    m_entries = entries;
    m_config = config;
    if (m_hwnd) {
        InvalidateRect(m_hwnd, NULL, FALSE);
    }
}

void FlyoutWindow::DrawChart(ID2D1RenderTarget* target, const D2D1_RECT_F& r, bool isDay) {
    // Background chart card
    ID2D1SolidColorBrush* boxBg = nullptr;
    ID2D1SolidColorBrush* boxBorder = nullptr;
    target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.04f) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.35f), &boxBg);
    target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.08f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f), &boxBorder);

    D2D1_ROUNDED_RECT chartBox = D2D1::RoundedRect(r, 6.0f, 6.0f);
    if (boxBg && boxBorder) {
        target->FillRoundedRectangle(chartBox, boxBg);
        target->DrawRoundedRectangle(chartBox, boxBorder, 1.0f);
        boxBg->Release();
        boxBorder->Release();
    }

    if (m_entries.empty()) return;

    std::vector<GlucoseEntry> pts = m_entries;
    if (pts.size() == 1) {
        GlucoseEntry prev = pts[0];
        prev.sgv -= prev.delta;
        pts.push_back(prev);
    }
    std::reverse(pts.begin(), pts.end());

    double minVal = 60.0;
    double maxVal = 200.0;
    for (const auto& p : pts) {
        if (p.sgv < minVal) minVal = p.sgv;
        if (p.sgv > maxVal) maxVal = p.sgv;
    }
    minVal = std::floor((minVal - 5.0) / 10.0) * 10.0;
    maxVal = std::ceil((maxVal + 5.0) / 10.0) * 10.0;
    double range = (maxVal > minVal) ? (maxVal - minVal) : 50.0;

    // Chart margins: left margin 30px for vertical labels, bottom 18px for time labels, right 8px, top 12px
    float padLeft = 32.0f;
    float padRight = 10.0f;
    float padTop = 10.0f;
    float padBottom = 20.0f;

    float chartX0 = r.left + padLeft;
    float chartX1 = r.right - padRight;
    float chartY0 = r.top + padTop;
    float chartY1 = r.bottom - padBottom;
    float w = chartX1 - chartX0;
    float h = chartY1 - chartY0;

    auto getY = [&](double sgv) -> float {
        float norm = (float)((sgv - minVal) / range);
        return chartY1 - (norm * h);
    };

    // Target Range Corridor (70 to 180)
    float yHigh = getY(m_config.alarmHigh);
    float yLow = getY(m_config.alarmLow);

    ID2D1SolidColorBrush* stripeBrush = nullptr;
    target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.6f, 0.2f, 0.08f) : D2D1::ColorF(0.0f, 1.0f, 0.2f, 0.06f), &stripeBrush);
    if (stripeBrush) {
        target->FillRectangle(D2D1::RectF(chartX0, (std::min)(yHigh, yLow), chartX1, (std::max)(yHigh, yLow)), stripeBrush);
        stripeBrush->Release();
    }

    // Grid / target lines
    ID2D1SolidColorBrush* gridBrush = nullptr;
    target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.12f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f), &gridBrush);
    if (gridBrush) {
        target->DrawLine(D2D1::Point2F(chartX0, yHigh), D2D1::Point2F(chartX1, yHigh), gridBrush, 1.0f);
        target->DrawLine(D2D1::Point2F(chartX0, yLow), D2D1::Point2F(chartX1, yLow), gridBrush, 1.0f);
    }

    // DirectWrite Text Format for Chart Labels
    IDWriteTextFormat* axisFormat = nullptr;
    m_d2d->DWriteFactory()->CreateTextFormat(
        L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, 8.5f, L"en-us", &axisFormat
    );

    ID2D1SolidColorBrush* labelBrush = nullptr;
    target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.40f, 0.40f, 0.40f) : D2D1::ColorF(0.65f, 0.65f, 0.65f), &labelBrush);

    bool isMmol = (m_config.unit == L"mmol");
    auto formatSgvLabel = [&](double v) -> std::wstring {
        std::wstringstream ss;
        if (isMmol) ss << std::fixed << std::setprecision(1) << (v / 18.0182);
        else ss << (int)std::round(v);
        return ss.str();
    };

    // Vertical Labels: maxVal, alarmHigh, alarmLow, minVal
    if (axisFormat && labelBrush) {
        axisFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);

        auto drawVLabel = [&](double val, float yPos) {
            std::wstring s = formatSgvLabel(val);
            target->DrawText(s.c_str(), (UINT32)s.length(), axisFormat,
                             D2D1::RectF(r.left + 2.0f, yPos - 6.0f, chartX0 - 4.0f, yPos + 8.0f), labelBrush);
        };

        drawVLabel(maxVal, chartY0);
        drawVLabel(m_config.alarmHigh, yHigh);
        drawVLabel(m_config.alarmLow, yLow);
        drawVLabel(minVal, chartY1);
    }

    // Draw Line Segments & Points colored by target range:
    // green inside target range [alarmLow, alarmHigh], orange outside target range
    D2D1_COLOR_F greenColor = isDay ? D2D1::ColorF(0.0f, 0.60f, 0.20f) : D2D1::ColorF(0.0f, 1.0f, 0.26f);
    D2D1_COLOR_F orangeColor = isDay ? D2D1::ColorF(0.85f, 0.46f, 0.0f) : D2D1::ColorF(1.0f, 0.60f, 0.0f);

    ID2D1SolidColorBrush* greenBrush = nullptr;
    ID2D1SolidColorBrush* orangeBrush = nullptr;
    target->CreateSolidColorBrush(greenColor, &greenBrush);
    target->CreateSolidColorBrush(orangeColor, &orangeBrush);

    auto getBrushForSgv = [&](double sgv) -> ID2D1SolidColorBrush* {
        if (sgv >= m_config.alarmLow && sgv <= m_config.alarmHigh) return greenBrush;
        return orangeBrush;
    };

    float stepX = (pts.size() > 1) ? (w / (float)(pts.size() - 1)) : w;

    // Draw line segments between adjacent points
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        float x1 = chartX0 + (float)i * stepX;
        float y1 = getY(pts[i].sgv);
        float x2 = chartX0 + (float)(i + 1) * stepX;
        float y2 = getY(pts[i + 1].sgv);

        // Segment color: if either point is outside target range, color orange; if both inside, green
        bool bothInside = (pts[i].sgv >= m_config.alarmLow && pts[i].sgv <= m_config.alarmHigh) &&
                          (pts[i + 1].sgv >= m_config.alarmLow && pts[i + 1].sgv <= m_config.alarmHigh);
        ID2D1SolidColorBrush* segBrush = bothInside ? greenBrush : orangeBrush;
        if (segBrush) {
            target->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), segBrush, 2.0f);
        }
    }

    // Draw point dots
    for (size_t i = 0; i < pts.size(); ++i) {
        float px = chartX0 + (float)i * stepX;
        float py = getY(pts[i].sgv);
        ID2D1SolidColorBrush* dotBrush = getBrushForSgv(pts[i].sgv);
        if (dotBrush) {
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(px, py), 2.2f, 2.2f), dotBrush);
        }
    }

    // Highlight latest reading
    if (!pts.empty()) {
        float lastX = chartX0 + (float)(pts.size() - 1) * stepX;
        float lastY = getY(pts.back().sgv);
        ID2D1SolidColorBrush* lastBrush = getBrushForSgv(pts.back().sgv);
        if (lastBrush) {
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(lastX, lastY), 4.5f, 4.5f), lastBrush);
        }
    }

    // Interactive Scrubber Tooltip
    if (m_hoveringChart && !pts.empty()) {
        // Find closest data point to m_mouseChartX
        size_t bestIdx = 0;
        float bestDiff = 999999.0f;
        for (size_t i = 0; i < pts.size(); ++i) {
            float px = chartX0 + (float)i * stepX;
            float diff = fabsf(m_mouseChartX - px);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = i;
            }
        }

        float ptX = chartX0 + (float)bestIdx * stepX;
        float ptY = getY(pts[bestIdx].sgv);

        // Draw vertical hairline scrubber
        ID2D1SolidColorBrush* scrubberLineBrush = nullptr;
        target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.35f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.50f), &scrubberLineBrush);
        if (scrubberLineBrush) {
            target->DrawLine(D2D1::Point2F(ptX, chartY0), D2D1::Point2F(ptX, chartY1), scrubberLineBrush, 1.0f);
            scrubberLineBrush->Release();
        }

        // Highlight selected dot
        ID2D1SolidColorBrush* ptBrush = getBrushForSgv(pts[bestIdx].sgv);
        if (ptBrush) {
            target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(ptX, ptY), 4.5f, 4.5f), ptBrush);
        }

        // Format tooltip text (stacked: Value \n HH:MM)
        std::wstringstream ttVal;
        if (isMmol) ttVal << std::fixed << std::setprecision(1) << (pts[bestIdx].sgv / 18.0182);
        else ttVal << (int)std::round(pts[bestIdx].sgv);

        time_t t = (time_t)(pts[bestIdx].date / 1000LL);
        struct tm ltm;
        localtime_s(&ltm, &t);
        wchar_t timeBuf[16];
        swprintf_s(timeBuf, L"%02d:%02d", ltm.tm_hour, ltm.tm_min);

        std::wstring lineVal = ttVal.str();
        std::wstring lineTime = std::wstring(timeBuf);

        // DirectWrite formats for tooltip badge
        IDWriteTextFormat* ttValFormat = nullptr;
        IDWriteTextFormat* ttTimeFormat = nullptr;
        m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.0f, L"en-us", &ttValFormat);
        m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f, L"en-us", &ttTimeFormat);
        if (ttValFormat) ttValFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        if (ttTimeFormat) ttTimeFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

        // Tooltip badge box geometry (width: 60px, height: 38px)
        float boxW = 60.0f;
        float boxH = 38.0f;
        float boxX = ptX + 6.0f;
        if (boxX + boxW > chartX1 + 4.0f) {
            boxX = ptX - boxW - 6.0f;
        }
        float boxY = chartY0 + 6.0f;

        D2D1_ROUNDED_RECT ttBox = D2D1::RoundedRect(D2D1::RectF(boxX, boxY, boxX + boxW, boxY + boxH), 5.0f, 5.0f);

        ID2D1SolidColorBrush* ttBgBrush = nullptr;
        ID2D1SolidColorBrush* ttBorderBrush = nullptr;
        ID2D1SolidColorBrush* ttTextBrush = nullptr;
        ID2D1SolidColorBrush* ttSubBrush = nullptr;

        target->CreateSolidColorBrush(isDay ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f) : D2D1::ColorF(0.08f, 0.08f, 0.09f, 0.92f), &ttBgBrush);
        target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.20f), &ttBorderBrush);
        target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f) : D2D1::ColorF(1.0f, 1.0f, 1.0f), &ttTextBrush);
        target->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.45f, 0.45f, 0.45f) : D2D1::ColorF(0.70f, 0.70f, 0.70f), &ttSubBrush);

        if (ttBgBrush && ttBorderBrush) {
            target->FillRoundedRectangle(ttBox, ttBgBrush);
            target->DrawRoundedRectangle(ttBox, ttBorderBrush, 1.0f);
        }

        if (ttTextBrush && ttValFormat) {
            target->DrawText(lineVal.c_str(), (UINT32)lineVal.length(), ttValFormat,
                             D2D1::RectF(boxX, boxY + 2.0f, boxX + boxW, boxY + 20.0f), ttTextBrush);
        }
        if (ttSubBrush && ttTimeFormat) {
            target->DrawText(lineTime.c_str(), (UINT32)lineTime.length(), ttTimeFormat,
                             D2D1::RectF(boxX, boxY + 20.0f, boxX + boxW, boxY + 36.0f), ttSubBrush);
        }

        if (ttBgBrush) ttBgBrush->Release();
        if (ttBorderBrush) ttBorderBrush->Release();
        if (ttTextBrush) ttTextBrush->Release();
        if (ttSubBrush) ttSubBrush->Release();
        if (ttValFormat) ttValFormat->Release();
        if (ttTimeFormat) ttTimeFormat->Release();
    }

    if (greenBrush) greenBrush->Release();
    if (orangeBrush) orangeBrush->Release();

    // Horizontal Time Labels (HH:MM format)
    if (axisFormat && labelBrush && !pts.empty()) {
        auto formatTime = [](long long tsMs) -> std::wstring {
            if (tsMs <= 0) return L"";
            time_t t = (time_t)(tsMs / 1000LL);
            struct tm ltm;
            localtime_s(&ltm, &t);
            wchar_t buf[16];
            swprintf_s(buf, L"%02d:%02d", ltm.tm_hour, ltm.tm_min);
            return std::wstring(buf);
        };

        // Draw oldest time on left
        axisFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        std::wstring tStart = formatTime(pts.front().date);
        target->DrawText(tStart.c_str(), (UINT32)tStart.length(), axisFormat,
                         D2D1::RectF(chartX0, chartY1 + 3.0f, chartX0 + 60.0f, chartY1 + 17.0f), labelBrush);

        // Draw midpoint time if available
        if (pts.size() >= 3) {
            axisFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            size_t midIdx = pts.size() / 2;
            std::wstring tMid = formatTime(pts[midIdx].date);
            float midX = chartX0 + (w / 2.0f);
            target->DrawText(tMid.c_str(), (UINT32)tMid.length(), axisFormat,
                             D2D1::RectF(midX - 30.0f, chartY1 + 3.0f, midX + 30.0f, chartY1 + 17.0f), labelBrush);
        }

        // Draw latest time on right
        axisFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        std::wstring tEnd = formatTime(pts.back().date);
        target->DrawText(tEnd.c_str(), (UINT32)tEnd.length(), axisFormat,
                         D2D1::RectF(chartX1 - 60.0f, chartY1 + 3.0f, chartX1, chartY1 + 17.0f), labelBrush);
    }

    if (gridBrush) gridBrush->Release();
    if (labelBrush) labelBrush->Release();
    if (axisFormat) axisFormat->Release();
}

void FlyoutWindow::OnPaint() {
    if (!m_renderTarget) return;

    m_renderTarget->BeginDraw();

    bool isDay = (m_config.theme == L"light");
    D2D1_COLOR_F bgColor = isDay ? D2D1::ColorF(0.96f, 0.96f, 0.98f, 0.98f) : D2D1::ColorF(0.11f, 0.11f, 0.12f, 0.98f);
    D2D1_COLOR_F borderColor = isDay ? D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.15f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f);

    m_renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));

    D2D1_SIZE_F size = m_renderTarget->GetSize();
    D2D1_ROUNDED_RECT rrect = D2D1::RoundedRect(D2D1::RectF(1.0f, 1.0f, size.width - 1.0f, size.height - 1.0f), 10.0f, 10.0f);

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

    // Title Header
    IDWriteTextFormat* titleFormat = nullptr;
    IDWriteTextFormat* heroFormat = nullptr;
    IDWriteTextFormat* unitFormat = nullptr;
    IDWriteTextFormat* deltaFormat = nullptr;
    IDWriteTextFormat* statsFormat = nullptr;

    m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.5f, L"en-us", &titleFormat);
    m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BLACK, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 36.0f, L"en-us", &heroFormat);
    // Units font (13.5pt semi-bold)
    m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13.5f, L"en-us", &unitFormat);
    // Delta format (12pt medium)
    m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_MEDIUM, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"en-us", &deltaFormat);
    m_d2d->DWriteFactory()->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 11.0f, L"en-us", &statsFormat);

    ID2D1SolidColorBrush* textBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.1f, 0.1f, 0.1f) : D2D1::ColorF(1.0f, 1.0f, 1.0f), &textBrush);

    ID2D1SolidColorBrush* mutedBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(isDay ? D2D1::ColorF(0.5f, 0.5f, 0.5f) : D2D1::ColorF(0.6f, 0.6f, 0.6f), &mutedBrush);

    if (textBrush && titleFormat) {
        std::wstring header = L"NIGHTSCOUT LIVE";
        m_renderTarget->DrawText(header.c_str(), (UINT32)header.length(), titleFormat, D2D1::RectF(16.0f, 12.0f, size.width - 65.0f, 26.0f), mutedBrush);
    }

    // Settings (Gear) icon in top-right area (cx: size.width - 44.0f, cy: 18.0f)
    {
        float gearCx = size.width - 44.0f;
        float gearCy = 18.0f;
        D2D1_COLOR_F gearColor = isDay ? D2D1::ColorF(0.55f, 0.55f, 0.55f) : D2D1::ColorF(0.55f, 0.55f, 0.55f);
        ID2D1SolidColorBrush* gearBrush = nullptr;
        m_renderTarget->CreateSolidColorBrush(gearColor, &gearBrush);
        if (gearBrush) {
            // Draw central hub
            m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(gearCx, gearCy), 4.5f, 4.5f), gearBrush, 1.5f);
            // Draw 6 cog spokes
            for (int i = 0; i < 6; ++i) {
                float rad = (float)i * (3.14159265f / 3.0f);
                float x1 = gearCx + cosf(rad) * 3.5f;
                float y1 = gearCy + sinf(rad) * 3.5f;
                float x2 = gearCx + cosf(rad) * 6.5f;
                float y2 = gearCy + sinf(rad) * 6.5f;
                m_renderTarget->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), gearBrush, 1.8f);
            }
            gearBrush->Release();
        }
    }

    // Pin icon in top-right corner (cx: size.width - 18.0f, cy: 18.0f)
    {
        float pinCx = size.width - 18.0f;
        float pinCy = 18.0f;
        D2D1_COLOR_F pinColor = m_isPinned
            ? (isDay ? D2D1::ColorF(0.0f, 0.45f, 0.85f) : D2D1::ColorF(0.20f, 0.65f, 1.0f))
            : (isDay ? D2D1::ColorF(0.60f, 0.60f, 0.60f) : D2D1::ColorF(0.45f, 0.45f, 0.45f));

        ID2D1SolidColorBrush* pinBrush = nullptr;
        m_renderTarget->CreateSolidColorBrush(pinColor, &pinBrush);
        if (pinBrush) {
            // Draw pushpin symbol
            // Pin head/body
            m_renderTarget->DrawLine(D2D1::Point2F(pinCx - 3.5f, pinCy - 4.0f), D2D1::Point2F(pinCx + 3.5f, pinCy - 4.0f), pinBrush, 1.8f);
            m_renderTarget->DrawLine(D2D1::Point2F(pinCx, pinCy - 4.0f), D2D1::Point2F(pinCx, pinCy + 1.0f), pinBrush, 2.2f);
            m_renderTarget->DrawLine(D2D1::Point2F(pinCx - 5.0f, pinCy + 1.0f), D2D1::Point2F(pinCx + 5.0f, pinCy + 1.0f), pinBrush, 1.8f);
            // Pin needle
            m_renderTarget->DrawLine(D2D1::Point2F(pinCx, pinCy + 1.0f), D2D1::Point2F(pinCx, pinCy + 6.0f), pinBrush, 1.5f);

            // If pinned, draw an indicator circle around it
            if (m_isPinned) {
                m_renderTarget->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(pinCx, pinCy), 9.0f, 9.0f), pinBrush, 1.2f);
            }
            pinBrush->Release();
        }
    }

    // Big Glucose Hero
    double currentSgv = m_entries.empty() ? 0.0 : m_entries[0].sgv;
    std::string direction = m_entries.empty() ? "Flat" : m_entries[0].direction;
    bool isMmol = (m_config.unit == L"mmol");

    std::wstringstream heroStream;
    if (currentSgv > 0) {
        if (isMmol) heroStream << std::fixed << std::setprecision(1) << (currentSgv / 18.0182);
        else heroStream << (int)currentSgv;
    } else {
        heroStream << L"--";
    }

    std::wstring heroStr = heroStream.str();
    D2D1_COLOR_F sgvColor = D2DContext::GetGlucoseColor(currentSgv > 0 ? currentSgv : 100.0, isDay, m_config.alarmLow, m_config.alarmHigh);
    ID2D1SolidColorBrush* heroBrush = nullptr;
    m_renderTarget->CreateSolidColorBrush(sgvColor, &heroBrush);

    float heroWidth = 55.0f;
    IDWriteTextLayout* heroLayout = nullptr;
    if (SUCCEEDED(m_d2d->DWriteFactory()->CreateTextLayout(
        heroStr.c_str(), (UINT32)heroStr.length(), heroFormat,
        200.0f, 50.0f, &heroLayout))) {
        DWRITE_TEXT_METRICS tm;
        if (SUCCEEDED(heroLayout->GetMetrics(&tm))) {
            heroWidth = tm.widthIncludingTrailingWhitespace;
        }
    }

    // Draw Hero number
    if (heroBrush) {
        if (heroLayout) {
            m_renderTarget->DrawTextLayout(D2D1::Point2F(16.0f, 28.0f), heroLayout, heroBrush);
            heroLayout->Release();
        } else if (heroFormat) {
            m_renderTarget->DrawText(heroStr.c_str(), (UINT32)heroStr.length(), heroFormat, D2D1::RectF(16.0f, 28.0f, 200.0f, 76.0f), heroBrush);
        }
    }

    // Trend Arrow between glucose value and delta/units:
    // Centered at cy=55.0f
    float arrowCx = 16.0f + heroWidth + 18.0f;
    float arrowCy = 55.0f;
    if (heroBrush) {
        D2DContext::DrawTrendArrow(m_renderTarget, heroBrush, direction, arrowCx, arrowCy, 33.0f, 3.5f);
        heroBrush->Release();
    }

    // Delta & Units block (switched order: line 1 = delta, line 2 = units)
    float unitLeft = arrowCx + 20.0f;

    // Line 1: Delta label with triangle symbol - moved 1px down to y=39 (y = 39 to 56)
    std::wstringstream deltaStream;
    deltaStream << L"\x0394 ";
    if (!m_entries.empty()) {
        double d = m_entries[0].delta;
        if (d > 0) deltaStream << L"+";
        if (isMmol) deltaStream << std::fixed << std::setprecision(1) << (d / 18.0182);
        else deltaStream << (int)d;
    } else {
        deltaStream << L"0";
    }
    std::wstring deltaStr = deltaStream.str();
    if (mutedBrush && deltaFormat) {
        m_renderTarget->DrawText(deltaStr.c_str(), (UINT32)deltaStr.length(), deltaFormat,
                                 D2D1::RectF(unitLeft, 39.0f, size.width - 16.0f, 56.0f), mutedBrush);
    }

    // Line 2: Units label - moved 1px up to y=53 (y = 53 to 70)
    std::wstring unitStr = isMmol ? L"mmol/L" : L"mg/dL";
    if (textBrush && unitFormat) {
        m_renderTarget->DrawText(unitStr.c_str(), (UINT32)unitStr.length(), unitFormat,
                                 D2D1::RectF(unitLeft, 53.0f, size.width - 16.0f, 70.0f), textBrush);
    }

    // 3-Hour Chart Area (with axis margins for labels) - extends to 286.0f, filling the bottom neatly
    DrawChart(m_renderTarget, D2D1::RectF(14.0f, 82.0f, size.width - 14.0f, 286.0f), isDay);

    if (textBrush) textBrush->Release();
    if (mutedBrush) mutedBrush->Release();
    if (titleFormat) titleFormat->Release();
    if (heroFormat) heroFormat->Release();
    if (unitFormat) unitFormat->Release();
    if (deltaFormat) deltaFormat->Release();
    if (statsFormat) statsFormat->Release();

    m_renderTarget->EndDraw();
}

LRESULT CALLBACK FlyoutWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    FlyoutWindow* pThis = (FlyoutWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        pThis = (FlyoutWindow*)cs->lpCreateParams;
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
        if (!pThis) break;
        int mouseX = LOWORD(lParam);
        int mouseY = HIWORD(lParam);

        // Check if user clicked the settings gear icon: x in [266, 288], y in [8, 30]
        if (mouseX >= 266 && mouseX <= 288 && mouseY >= 8 && mouseY <= 30) {
            if (pThis->m_onSettingsClick) {
                pThis->m_onSettingsClick();
            }
            return 0;
        }

        // Check if user clicked the pin icon: x in [292, 314], y in [8, 30]
        if (mouseX >= 292 && mouseX <= 314 && mouseY >= 8 && mouseY <= 30) {
            pThis->m_isPinned = !pThis->m_isPinned;
            if (pThis->m_isPinned) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        break;
    }
    case WM_MOUSEMOVE: {
        if (!pThis) break;
        int mouseX = LOWORD(lParam);
        int mouseY = HIWORD(lParam);

        // Chart bounds: x in [14, 306], y in [82, 286]
        if (mouseX >= 14 && mouseX <= 306 && mouseY >= 82 && mouseY <= 286) {
            pThis->m_hoveringChart = true;
            pThis->m_mouseChartX = (float)mouseX;
        } else {
            pThis->m_hoveringChart = false;
        }

        if (!pThis->m_trackingMouse) {
            TRACKMOUSEEVENT tme = { sizeof(tme) };
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            pThis->m_trackingMouse = true;
        }

        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (pThis) {
            pThis->m_hoveringChart = false;
            pThis->m_trackingMouse = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_ACTIVATE: {
        if (LOWORD(wParam) == WA_INACTIVE) {
            if (pThis && !pThis->m_isPinned) {
                pThis->Hide();
            }
        }
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}



