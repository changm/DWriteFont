#pragma once

#include <dwrite.h>
#include <dwrite_1.h>
#include <direct.h>
#include <d3d11.h>
#include <d2d1.h>
#include "SkMaskGamma.h"
#include <Wincodec.h>
#include <d2d1_1.h>

//The following typedef hides from the rest of the implementation the number of
//most significant bits to consider when creating mask gamma tables. Two bits
//per channel was chosen as a balance between fidelity (more bits) and cache
//sizes (fewer bits). Three bits per channel was chosen when #303942; (used by
//the Chrome UI) turned out too green.
typedef SkTMaskGamma<3, 3, 3> SkMaskGamma;

class D2DSetup
{
public:
    D2DSetup(HWND aHWND)
        : fPreBlend(CreateLUT())
        , fGdiPreBlend(CreateGdiLUT())
    {
        mHWND = aHWND;
        Init();
    }

    ~D2DSetup();

    void DrawText(int x, int y, WCHAR message[]);
    void Init();
    void InitD3D();
    void InitD2D();
    void DrawWithMask();
    void CreateHardwareRenderTarget();
    void AlternateText(int count);
    void PrintFonts(IDWriteFontCollection* aFontCollection);
    void Clear();
    void DrawLuminanceEffect();
    void Present();
    void CreateImageBrushes();
    void InitDWrite();

private:
    SkMaskGamma::PreBlend CreateLUT();
    SkMaskGamma::PreBlend CreateGdiLUT();

    IDWriteFontFace* GetFontFace();
    void CreateGlyphRun(DWRITE_GLYPH_RUN& glyphRun, IDWriteFontFace* fontFace, WCHAR message[], float aScale = 1.0);

    BYTE* ConvertToBGRA(BYTE* aRGB, int width, int height, bool useLUT, bool convert = false, bool useGDILUT = false);
    BYTE* BlendSkiaGrayscale(BYTE* aRGB, int width, int height);
    BYTE* BlitDirectly(BYTE* aRGB, int width, int height);

    void DrawBitmap(BYTE* image, float width, float height, int x, int y, RECT bounds);

    void DrawGrayscaleWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y);
    void DrawGrayscaleWithLUT(DWRITE_GLYPH_RUN& glyphRun, int x, int y);
    void DrawWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y, bool useLUT, bool convert = false,
        DWRITE_RENDERING_MODE aRenderingMode = DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC,
        DWRITE_MEASURING_MODE aMode = DWRITE_MEASURING_MODE_NATURAL,
        bool aClear = false,
        bool useGDILUT = false);
    void DrawTextWithD2D(DWRITE_GLYPH_RUN& glyphRun, int x, int y,
        IDWriteRenderingParams* aParams, bool aClear = false,
        D2D1_TEXT_ANTIALIAS_MODE aaMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
    void CreateBitmap(ID2D1RenderTarget* aRenderTarget, ID2D1Bitmap** aOutBitmap,
                     int width, int height,
                     BYTE* aSource = nullptr, uint32_t aSourceStride = 0);

    BYTE* GetAlphaTexture(DWRITE_GLYPH_RUN& aRun, RECT& aOutBounds,
                        DWRITE_RENDERING_MODE aRenderMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL,
                        DWRITE_MEASURING_MODE aMeasureMode = DWRITE_MEASURING_MODE_NATURAL);

    void GetGlyphBounds(DWRITE_GLYPH_RUN& aRun, RECT& aOutBounds,
                        IDWriteGlyphRunAnalysis** aOutAnalysis,
                        DWRITE_RENDERING_MODE aRenderMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL,
                        DWRITE_MEASURING_MODE aMeasureMode = DWRITE_MEASURING_MODE_NATURAL);
    float GetScaleFactor() { return mDpiX / 96.0f; }
    void PrintElapsedTime(LARGE_INTEGER aStart, LARGE_INTEGER aEnd, const char* aMsg);

    void CreateD3DDevice();
    void CreateD2DDevices();
    void CreateDXGIResources();
    void SetD2DToBackBuffer();
    void ReleaseDWrite();
    void ReleaseBrushes();
    void ReleaseD3D();
    void ReleaseD2D();
    void CleanWICResources();
    void PrintAlphaBitmap(ID2D1Bitmap1* aBitmap);
    void PrintAlphaBitmapWithCreateReadback(ID2D1Bitmap1* aBitmap);
    void PushLayer(ID2D1Image* aMaskImage);
    void PopLayer();
    void PrintTargetBitmap(D2D1_SIZE_U aBitmapSize);

    HWND mHWND;

    ID2D1Factory1* mFactory;
    ID2D1HwndRenderTarget* mRenderTarget;
    ID2D1RenderTarget* mBitmapRenderTarget;

    ID2D1Device* md2d_device;
    ID2D1DeviceContext* mDC;

    IDWriteFactory* mDwriteFactory;
    IDWriteTextFormat* mTextFormat;
    IDWriteRenderingParams* mCustomParams;
    IDWriteRenderingParams* mDefaultParams;
    IDWriteRenderingParams* mGDIParams;
    IDWriteRenderingParams* mGrayscaleParams;

    ID2D1SolidColorBrush* mBlackBrush;
    ID2D1SolidColorBrush* mWhiteBrush;
    ID2D1SolidColorBrush* mDarkBlackBrush;
    ID2D1SolidColorBrush* mRedBrush;
    ID2D1SolidColorBrush* mTransparentBlackBrush;

    float mFontSize;

    SkMaskGamma::PreBlend fPreBlend;
    SkMaskGamma::PreBlend fGdiPreBlend;

    IWICImagingFactory* mWICFactory;
    IWICBitmap* mWICBitmap;

    float mDpiX;
    float mDpiY;

    LARGE_INTEGER mFrequency;

    // D2D 11 things
    ID3D11Device* mD3D_Device;
    ID3D11DeviceContext* mD3D_DeviceContext;
    IDXGISwapChain* mSwapChain;
    IDXGIAdapter* mAdapter;
    IDXGISurface1* mDxgiSurface;
    IDXGIFactory* mDxgiFactory;
    ID2D1Bitmap1* mTargetBitmap;
    IDXGIDevice* mDxgiDevice;
};
