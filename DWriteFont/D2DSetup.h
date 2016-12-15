#pragma once

#include <dwrite.h>
#include <dwrite_1.h>
#include <direct.h>
#include <d3d11.h>
#include <d2d1.h>
#include "SkMaskGamma.h"
#include <Wincodec.h>

//The following typedef hides from the rest of the implementation the number of
//most significant bits to consider when creating mask gamma tables. Two bits
//per channel was chosen as a balance between fidelity (more bits) and cache
//sizes (fewer bits). Three bits per channel was chosen when #303942; (used by
//the Chrome UI) turned out too green.
typedef SkTMaskGamma<3, 3, 3> SkMaskGamma;

class D2DSetup
{
public:
    D2DSetup(HWND aHWND, HDC aHDC)
        : fPreBlend(CreateLUT())
        , fGdiPreBlend(CreateGdiLUT())
    {
        mHDC = aHDC;
        mHWND = aHWND;
        Init();
    }

    ~D2DSetup();

    void DrawText(int x, int y, WCHAR message[]);
    void Init();
    void DrawWithMask();
    void AlternateText(int count);
    void PrintFonts(IDWriteFontCollection* aFontCollection);
    void Clear();

private:
    SkMaskGamma::PreBlend CreateLUT();
    SkMaskGamma::PreBlend CreateGdiLUT();

    IDWriteFontFace* GetFontFace();
    void DrawTextWithD2D(DWRITE_GLYPH_RUN& glyphRun, int x, int y,
        IDWriteRenderingParams* aParams, bool aClear = false,
        D2D1_TEXT_ANTIALIAS_MODE aaMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    void CreateGlyphRunAnalysis(DWRITE_GLYPH_RUN& glyphRun, IDWriteFontFace* fontFace, WCHAR message[]);

    BYTE* ConvertToRGBA(BYTE* aRGB, int width, int height, bool useLUT, bool convert = false, bool useGDILUT = false);
    BYTE* BlendGrayscale(BYTE* aRGB, int width, int height);
    BYTE* BlendRaw(BYTE* aRGB, int width, int height);

    void DrawBitmap(BYTE* image, float width, float height, int x, int y, RECT bounds);
    void DrawGrayscaleWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y);

    void DrawWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y, bool useLUT, bool convert = false,
        DWRITE_RENDERING_MODE aRenderingMode = DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
        DWRITE_MEASURING_MODE aMode = DWRITE_MEASURING_MODE_NATURAL,
        bool aClear = false,
        bool useGDILUT = false);

    HWND mHWND;
    HDC mHDC;

    ID2D1Factory* mFactory;
    ID2D1HwndRenderTarget* mRenderTarget;
    ID2D1RenderTarget* mBitmapRenderTarget;

    IDWriteFactory* mDwriteFactory;
    IDWriteTextFormat* mTextFormat;
    ID2D1SolidColorBrush* mBlackBrush;
    ID2D1SolidColorBrush* mWhiteBrush;
    ID2D1SolidColorBrush* mDarkBlackBrush;
    ID2D1SolidColorBrush* mTransparentBlackBrush;

    IDWriteRenderingParams* mCustomParams;
    IDWriteRenderingParams* mDefaultParams;
    IDWriteRenderingParams* mGDIParams;
    IDWriteRenderingParams* mGrayscaleParams;
    int mFontSize;

    SkMaskGamma::PreBlend fPreBlend;
    SkMaskGamma::PreBlend fGdiPreBlend;

    IWICImagingFactory* mWICFactory;
    IWICBitmap* mWICBitmap;
};
