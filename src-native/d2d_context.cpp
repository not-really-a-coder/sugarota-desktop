#include "d2d_context.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

D2DContext::D2DContext() {}
D2DContext::~D2DContext() {
    if (m_dwriteFactory) m_dwriteFactory->Release();
    if (m_d2dFactory) m_d2dFactory->Release();
}

bool D2DContext::Init() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_d2dFactory);
    if (FAILED(hr)) return false;

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(&m_dwriteFactory));
    return SUCCEEDED(hr);
}

D2D1_COLOR_F D2DContext::GetGlucoseColor(double sgv, bool isDay, double low, double high) {
    if (isDay) {
        if (sgv < low) return D2D1::ColorF(0.80f, 0.0f, 0.0f);     // Dark Red
        if (sgv <= 70.0) return D2D1::ColorF(0.85f, 0.46f, 0.0f);  // Dark Orange
        if (sgv <= high) return D2D1::ColorF(0.0f, 0.60f, 0.20f);  // Dark Green
        if (sgv <= 250.0) return D2D1::ColorF(0.85f, 0.46f, 0.0f); // Dark Orange
        return D2D1::ColorF(0.80f, 0.0f, 0.0f);                    // Dark Red
    }

    if (sgv < low) return D2D1::ColorF(1.0f, 0.26f, 0.26f);      // Red
    if (sgv <= 70.0) return D2D1::ColorF(1.0f, 0.60f, 0.0f);     // Orange
    if (sgv <= high) return D2D1::ColorF(0.0f, 1.0f, 0.26f);     // Neon Green
    if (sgv <= 250.0) return D2D1::ColorF(1.0f, 0.60f, 0.0f);    // Orange
    return D2D1::ColorF(1.0f, 0.26f, 0.26f);                     // Red
}

void D2DContext::DrawTrendArrow(ID2D1RenderTarget* target, ID2D1Brush* brush, const std::string& direction, float cx, float cy, float size, float strokeWidth) {
    float angle = 0.0f;
    bool isDouble = false;

    if (direction == "DoubleUp") {
        angle = -90.0f;
        isDouble = true;
    } else if (direction == "SingleUp") {
        angle = -90.0f;
    } else if (direction == "FortyFiveUp") {
        angle = -45.0f;
    } else if (direction == "Flat") {
        angle = 0.0f;
    } else if (direction == "FortyFiveDown") {
        angle = 45.0f;
    } else if (direction == "SingleDown") {
        angle = 90.0f;
    } else if (direction == "DoubleDown") {
        angle = 90.0f;
        isDouble = true;
    }

    D2D1_MATRIX_3X2_F oldTrans;
    target->GetTransform(&oldTrans);

    D2D1_MATRIX_3X2_F rot = D2D1::Matrix3x2F::Rotation(angle, D2D1::Point2F(cx, cy));
    target->SetTransform(rot * oldTrans);

    float half = size * 0.44f;
    float tipLen = size * 0.35f;

    auto drawSingle = [&](float offsetY) {
        float y = cy + offsetY;
        target->DrawLine(D2D1::Point2F(cx - half, y), D2D1::Point2F(cx + half, y), brush, strokeWidth);
        target->DrawLine(D2D1::Point2F(cx + half, y), D2D1::Point2F(cx + half - tipLen, y - tipLen), brush, strokeWidth);
        target->DrawLine(D2D1::Point2F(cx + half, y), D2D1::Point2F(cx + half - tipLen, y + tipLen), brush, strokeWidth);
    };

    if (isDouble) {
        float sep = (size > 20.0f) ? 5.5f : 3.2f;
        drawSingle(-sep);
        drawSingle(sep);
    } else {
        drawSingle(0.0f);
    }

    target->SetTransform(oldTrans);
}

