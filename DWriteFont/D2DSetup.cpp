
#include "stdafx.h"
#include "D2DSetup.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <assert.h>
#include <stdio.h>

D2DSetup::~D2DSetup()
{
	mFactory->Release();
	mRenderTarget->Release();
	mDwriteFactory->Release();
	mTextFormat->Release();
	mBlackBrush->Release();
	mTransparentBlackBrush->Release();
	mWhiteBrush->Release();
}

void D2DSetup::DrawText()
{
	static const WCHAR message[] = L"Hello World";

	mRenderTarget->BeginDraw();
	mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

	mRenderTarget->DrawTextW(message,
		ARRAYSIZE(message) - 1,
		mTextFormat,
		D2D1::RectF(0, 0, 500, 500),
		mBlackBrush);

	mRenderTarget->EndDraw();
}

void D2DSetup::PrintFonts(IDWriteFontCollection* aFontCollection)
{
	UINT32 familyCount = aFontCollection->GetFontFamilyCount();
	for (int i = 0; i < familyCount; i++) {
		IDWriteFontFamily* temp;
		aFontCollection->GetFontFamily(i, &temp);

		IDWriteLocalizedStrings* familyName;
		temp->GetFamilyNames(&familyName);

		UINT32 index = 0;
		BOOL exists;
		wchar_t localName[256];
		familyName->FindLocaleName(L"en-us", &index, &exists);

		UINT32 length = 0;
		familyName->GetStringLength(index, &length);

		wchar_t fontName[256];
		familyName->GetString(index, fontName, length + 1);
		wprintf(L"Font name: %s\n", fontName);
	}
}

static int
Blend(float src, float dst, float alpha) {
	float floatAlpha = alpha / 255;
	//return dst + ((src - dst) * alpha >> 8);
	float result = (src * floatAlpha) + (dst * (1 - floatAlpha));
	return result;
}

void D2DSetup::DrawWithMask()
{
	static const WCHAR message[] = L"Hello";
	static const WCHAR fontFamilyName[] = L"Consolas";

	int length = ARRAYSIZE(message) - 1;
	mRenderTarget->BeginDraw();
	mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

	IDWriteFontCollection* systemFonts;
	mDwriteFactory->GetSystemFontCollection(&systemFonts, TRUE);

	UINT32 fontIndex;
	BOOL exists;
	HRESULT hr = systemFonts->FindFamilyName(fontFamilyName, &fontIndex, &exists);
	assert(exists);

	IDWriteFontFamily* fontFamily;
	systemFonts->GetFontFamily(fontIndex, &fontFamily);

	IDWriteFont* actualFont;
	fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &actualFont);

	IDWriteFontFace* fontFace;
	actualFont->CreateFontFace(&fontFace);

	FLOAT advance = 0;

	// Map our things
	UINT16 glyphIndices[6];
	UINT32 textLength = 6;
	UINT32 codePoints[6];
	DWRITE_GLYPH_METRICS metrics[6];
	FLOAT advances[6];

	for (int i = 0; i < 6; i++) {
		codePoints[i] = message[i];
	}
	fontFace->GetGlyphIndicesW(codePoints, textLength, glyphIndices);

	fontFace->GetDesignGlyphMetrics(glyphIndices, textLength, metrics);
	for (int i = 0; i < 6; i++) {
		//advances[i] = metrics[i].advanceWidth / 96.0;
		//printf("Advance is: %f\n", advances[i]);
		advances[i] = 15;
	}

	DWRITE_GLYPH_OFFSET offset[6];
	for (int i = 0; i < 6; i++) {
		offset[i].advanceOffset = 0;
		offset[i].ascenderOffset = 0;
	}

	DWRITE_GLYPH_RUN glyphRun;
	glyphRun.glyphCount = 5;
	glyphRun.glyphAdvances = advances;
	glyphRun.fontFace = fontFace;
	glyphRun.fontEmSize = 28;
	glyphRun.bidiLevel = 0;
	glyphRun.glyphIndices = glyphIndices;
	glyphRun.isSideways = FALSE;
	glyphRun.glyphOffsets = offset;

	IDWriteGlyphRunAnalysis* analysis;
	// The 1.0f could be pretty bad here since it's not accounting for DPI
	mDwriteFactory->CreateGlyphRunAnalysis(&glyphRun, 1.0f, NULL, DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &analysis);
	RECT bounds;
	analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);

	float width = bounds.right - bounds.left;
	float height = bounds.bottom - bounds.top;
	int bufferSize = width * height * 3;
	BYTE* image = (BYTE*)malloc(bufferSize);
	memset(image, 0xFF, bufferSize);
	hr = analysis->CreateAlphaTexture(DWRITE_TEXTURE_TYPE::DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, image, bufferSize);
	assert(hr == S_OK);

	BYTE* bitmapImage = (BYTE*)malloc(width * height * 4);
	memset(bitmapImage, 0x00, (width * height * 4));
	for (int y = 0; y < height; y++) {
		int threeHeight = y * width * 3;
		int fourHeight = y * width * 4;
		for (int i = 0; i < width; i++) {
			int fourIndex = fourHeight + (4 * i);
			int threeIndex = threeHeight + (i * 3);
			// Invert to be black on white
			int r = image[threeIndex];
			int g = image[threeIndex + 1];
			int b = image[threeIndex + 2];


			r = Blend(0, 0xFF, r);
			g = Blend(0, 0xFF, g);
			b = Blend(0, 0xFF, b);

			/*
			r = Blend(0xFF, 0, r);
			g = Blend(0xFF, 0, g);
			b = Blend(0xFF, 0, b);
			*/

			bitmapImage[fourIndex] = r;
			bitmapImage[fourIndex + 1] = g;
			bitmapImage[fourIndex + 2] = b;
			bitmapImage[fourIndex + 3] = 0xFF;
		}
	}

	// Now try to make a bitmap
	ID2D1Bitmap* bitmap;
	//D2D1_BITMAP_PROPERTIES properties = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED };
	D2D1_BITMAP_PROPERTIES properties = { DXGI_FORMAT_B8G8R8A8_UNORM,  D2D1_ALPHA_MODE_PREMULTIPLIED };
	D2D1_SIZE_U size = D2D1::SizeU(width, height);

	hr = mRenderTarget->CreateBitmap(size, bitmapImage, width * 4, properties, &bitmap);
	if (hr != S_OK) {
		printf("Last error is: %d\n", GetLastError());
	}
	assert(hr == S_OK);

	D2D1_RECT_F destRect;
	destRect.left = 100 + bounds.left;
	destRect.right = 100 + bounds.right;
	destRect.top = 100 + bounds.top;
	destRect.bottom = 100+ bounds.bottom;

	D2D1_RECT_F srcRect;
	srcRect.left = 0;
	srcRect.right = width;
	srcRect.top = 0;
	srcRect.bottom = height;

	mRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	//mRenderTarget->FillOpacityMask(bitmap, mBlackBrush, D2D1_OPACITY_MASK_CONTENT_TEXT_NATURAL, &destRect, &srcRect);
	mRenderTarget->DrawBitmap(bitmap, &destRect, 1.0, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, &srcRect);
	//mRenderTarget->DrawBitmap(bitmap, &destRect, 1.0);

	D2D1_POINT_2F origin;
	origin.x = 165;
	origin.y = 200;
	mRenderTarget->DrawGlyphRun(origin, &glyphRun, mBlackBrush);

	mRenderTarget->EndDraw();
}

static
HMONITOR GetPrimaryMonitorHandle()
{
	const POINT ptZero = { 0, 0 };
	return MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
}

void D2DSetup::Init()
{
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &mFactory);
	assert(hr == S_OK);

	RECT clientRect;
	GetClientRect(mHWND, &clientRect);

	D2D1_SIZE_U size = D2D1::SizeU(
		clientRect.right - clientRect.left,
		clientRect.bottom - clientRect.top
	);

	// Create a Direct2D render target.
	D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties();
	D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProperties = D2D1::HwndRenderTargetProperties(mHWND, size);
	hr = mFactory->CreateHwndRenderTarget(&properties, &hwndProperties,
										  &mRenderTarget);
	assert(hr == S_OK);

	hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&mDwriteFactory));
	assert(hr == S_OK);

	hr = mDwriteFactory->CreateTextFormat(L"Consolas", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 28, L"", &mTextFormat);
	assert(hr == S_OK);

	mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &mBlackBrush);
	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &mWhiteBrush);
	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.0f), &mTransparentBlackBrush);

	// Now we can play with params
	mRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);

	IDWriteRenderingParams* defaultParams;
	mDwriteFactory->CreateRenderingParams(&defaultParams);

	IDWriteRenderingParams* customParams;
	mDwriteFactory->CreateCustomRenderingParams(defaultParams->GetGamma(), defaultParams->GetEnhancedContrast(), defaultParams->GetClearTypeLevel(), defaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, &customParams);

	IDWriteRenderingParams* monitorParams;
	mDwriteFactory->CreateMonitorRenderingParams(GetPrimaryMonitorHandle(), &monitorParams);
	mRenderTarget->SetTextRenderingParams(defaultParams);
}