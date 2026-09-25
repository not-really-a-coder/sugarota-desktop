#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <string>
#include <vector>
#include "config.h"

class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    bool Init();
    ID2D1Factory* Factory() const { return m_d2dFactory; }
    IDWriteFactory* DWriteFactory() const { return m_dwriteFactory; }

    static D2D1_COLOR_F GetGlucoseColor(double sgv, bool isDay, double low = 70.0, double high = 180.0);
    static void DrawTrendArrow(ID2D1RenderTarget* target, ID2D1Brush* brush, const std::string& direction, float cx, float cy, float size, float strokeWidth = 2.0f);

private:
    ID2D1Factory* m_d2dFactory = nullptr;
    IDWriteFactory* m_dwriteFactory = nullptr;
};
