
#include "stdafx.h"
#include "D2DSetup.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <assert.h>
#include <stdio.h>

#define SK_A32_SHIFT 24
#define SK_R32_SHIFT 16
#define SK_G32_SHIFT 8
#define SK_B32_SHIFT 0

/**
*  Pack the components into a SkPMColor, checking (in the debug version) that
*  the components are 0..255, and are already premultiplied (i.e. alpha >= color)
*/
static inline int SkPackARGB32(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
	return (a << SK_A32_SHIFT) | (r << SK_R32_SHIFT) |
			(g << SK_G32_SHIFT) | (b << SK_B32_SHIFT);
}

D2DSetup::~D2DSetup()
{
	mFactory->Release();
	mRenderTarget->Release();
	mDwriteFactory->Release();
	mTextFormat->Release();
	mBlackBrush->Release();
	mTransparentBlackBrush->Release();
	mWhiteBrush->Release();
	mDefaultParams->Release();
	mCustomParams->Release();

}

void D2DSetup::Clear()
{
	mRenderTarget->BeginDraw();
	mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
	mRenderTarget->EndDraw();
}

void D2DSetup::DrawText(int x, int y, WCHAR message[])
{
	const int length = wcslen(message);
	mRenderTarget->BeginDraw();

	mRenderTarget->DrawTextW(message,
		length,
		mTextFormat,
		D2D1::RectF(x, y - 17, 1000, 1000),
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

static inline int SkBlend32(int src, int dst, int alpha) {
	return dst + ((src - dst) * alpha >> 5);
}

IDWriteFontFace* D2DSetup::GetFontFace()
{
	static const WCHAR fontFamilyName[] = L"Arial";

	IDWriteFontCollection* systemFonts;
	mDwriteFactory->GetSystemFontCollection(&systemFonts, TRUE);
	//PrintFonts(systemFonts);

	UINT32 fontIndex;
	BOOL exists;
	HRESULT hr = systemFonts->FindFamilyName(fontFamilyName, &fontIndex, &exists);
	assert(exists);

	IDWriteFontFamily* fontFamily;
	systemFonts->GetFontFamily(fontIndex, &fontFamily);

	IDWriteFont* actualFont;
	fontFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &actualFont);

	IDWriteLocalizedStrings* names;
	BOOL info_string_exists = false;
	hr = actualFont->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_FULL_NAME, &names, &info_string_exists);
	assert(hr == S_OK);

	int count = names->GetCount();

	uint32_t length;
	names->GetStringLength(0, &length);
	assert(hr == S_OK);

	length += 1; // Need for null terminating char
	WCHAR* buffer = new WCHAR[length];
	hr = names->GetString(0, buffer, length);
	assert(hr == S_OK);

	wprintf(L"String is: %s\n", buffer);

	printf("Stretch: %d, Style: %d, Weight: %d\n",
		actualFont->GetStretch(), actualFont->GetStyle(), actualFont->GetWeight());

	IDWriteFontFace* fontFace;
	actualFont->CreateFontFace(&fontFace);

	return fontFace;
}

void D2DSetup::CreateGlyphRunAnalysis(DWRITE_GLYPH_RUN& glyphRun, IDWriteFontFace* fontFace, WCHAR message[])
{
	//static const WCHAR message[] = L"Hello World Glyph";
	const int length = wcslen(message);

	UINT16* glyphIndices = new UINT16[length];
	UINT32* codePoints = new UINT32[length];
	DWRITE_GLYPH_METRICS* glyphMetrics = new DWRITE_GLYPH_METRICS[length];
	FLOAT* advances = new FLOAT[length];

	for (int i = 0; i < length; i++) {
		codePoints[i] = message[i];
	}

	fontFace->GetGlyphIndicesW(codePoints, length, glyphIndices);
	fontFace->GetDesignGlyphMetrics(glyphIndices, length, glyphMetrics);

	DWRITE_FONT_METRICS fontMetrics;
	fontFace->GetMetrics(&fontMetrics);

	for (int i = 0; i < length; i++) {
		int advance = glyphMetrics[i].advanceWidth;
		float realAdvance = ((float) advance * mFontSize) / fontMetrics.designUnitsPerEm;
		advances[i] = realAdvance;
	}

	DWRITE_GLYPH_OFFSET* offset = new DWRITE_GLYPH_OFFSET[length];
	for (int i = 0; i < length; i++) {
		offset[i].advanceOffset = 0;
		offset[i].ascenderOffset = 0;
	}

	glyphRun.glyphCount = length;
	glyphRun.glyphAdvances = advances;
	glyphRun.fontFace = fontFace;
	glyphRun.fontEmSize = mFontSize;
	glyphRun.bidiLevel = 0;
	glyphRun.glyphIndices = glyphIndices;
	glyphRun.isSideways = FALSE;
	glyphRun.glyphOffsets = offset;
}

void D2DSetup::DrawTextWithD2D(DWRITE_GLYPH_RUN& glyphRun, int x, int y, IDWriteRenderingParams* aParams, bool aClear)
{
	D2D1_POINT_2F origin;
	origin.x = x;
	origin.y = y;

	mRenderTarget->BeginDraw();
	mRenderTarget->SetTextRenderingParams(aParams);
	mRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	mRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

	if (aClear) {
		mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
	}

	mRenderTarget->DrawGlyphRun(origin, &glyphRun, mBlackBrush);
	mRenderTarget->EndDraw();
}

static inline int SkUpscale31To32(int value) {
	return value + (value >> 4);
}

// Converts the given rgb 3x1 cleartype alpha mask to the required RGBA_UNOM as required by bitmaps
// Also blends to draw black text on white
BYTE* D2DSetup::ConvertToRGBA(BYTE* aRGB, int width, int height, bool useLUT, bool convert, bool useGDILUT)
{
	int size = width * height * 4;
	BYTE* bitmapImage = (BYTE*)malloc(size);
	memset(bitmapImage, 0x00, size);

	const uint8_t* tableR = this->fPreBlend.fR;
	const uint8_t* tableG = this->fPreBlend.fG;
	const uint8_t* tableB = this->fPreBlend.fB;

	if (useGDILUT) {
		tableR = this->fGdiPreBlend.fR;
		tableG = this->fGdiPreBlend.fG;
		tableB = this->fGdiPreBlend.fB;
	}

	printf("Final output\n\n");

	for (int y = 0; y < height; y++) {
		int sourceHeight = y * width * 3;	// expect 3 bytes per pixel
		int destHeight = y * width * 4;		// convert to 4 bytes per pixel to add alpha opaque channel

		for (int i = 0; i < width; i++) {
			int destIndex = destHeight + (4 * i);
			int srcIndex = sourceHeight + (i * 3);

			BYTE r = aRGB[srcIndex];
			BYTE g = aRGB[srcIndex + 1];
			BYTE b = aRGB[srcIndex + 2];

			BYTE orig_r = r;
			BYTE orig_g = g;
			BYTE orig_b = b;

			BYTE lut_r;
			BYTE lut_g;
			BYTE lut_b;

			BYTE short_r;
			BYTE short_g;
			BYTE short_b;

			if (useLUT) {
				lut_r = sk_apply_lut_if<true>(r, tableR);
				lut_g = sk_apply_lut_if<true>(g, tableG);
				lut_b = sk_apply_lut_if<true>(b, tableB);

				r = sk_apply_lut_if<true>(r, tableR);
				g = sk_apply_lut_if<true>(g, tableG);
				b = sk_apply_lut_if<true>(b, tableB);
			}

			if (convert) {
				// Taken from http://searchfox.org/mozilla-central/source/gfx/skia/skia/include/core/SkColorPriv.h#654
				short_r = r >> 3;
				short_g = g >> 3;
				short_b = b >> 3;

				r = short_r << 3;
				g = short_g << 3;
				b = short_b << 3;
			}

			BYTE upscale_r = SkUpscale31To32(short_r);
			BYTE upscale_g = SkUpscale31To32(short_g);
			BYTE upscale_b = SkUpscale31To32(short_b);

			BYTE skia_blend_r = SkBlend32(0, 255, upscale_r);
			BYTE skia_blend_g = SkBlend32(0, 255, upscale_g);
			BYTE skia_blend_b = SkBlend32(0, 255, upscale_b);

			// Blend to draw black text on white
			/*
			r = Blend(0x00, 0xFF, r);
			g = Blend(0x00, 0xFF, g);
			b = Blend(0x00, 0xFF, b);

			bitmapImage[destIndex] = r;
			bitmapImage[destIndex + 1] = g;
			bitmapImage[destIndex + 2] = b;
			bitmapImage[destIndex + 3] = 0xFF;
			*/

			bitmapImage[destIndex] = skia_blend_r;
			bitmapImage[destIndex + 1] = skia_blend_g;
			bitmapImage[destIndex + 2] = skia_blend_b;
			bitmapImage[destIndex + 3] = 0xFF;

			/*
			printf("Orig (%u, %u, %u) => LUT (%u, %u, %u) => Short (%u, %u, %u) => Upscale (%u, %u, %u) => final (%u, %u, %u)\n",
				orig_r, orig_g, orig_b,
				lut_r, lut_g, lut_b,
				short_r, short_g, short_b,
				upscale_r, upscale_g, upscale_b,
				skia_blend_r, skia_blend_g, skia_blend_b);
				*/
		}
		//printf("\n");

	}
	return bitmapImage;
}

void D2DSetup::DrawWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y, bool useLUT, bool convert,
							  DWRITE_RENDERING_MODE aRenderMode, DWRITE_MEASURING_MODE aMeasureMode,
							  bool aClear, bool useGDILUT)
{
	mRenderTarget->BeginDraw();

	if (aClear) {
		mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
	}

	IDWriteGlyphRunAnalysis* analysis;
	DWRITE_MATRIX fXform;
	fXform.m11 = 1.0;
	fXform.m12 = 0.0;
	fXform.m21 = 0.0;
	fXform.m22 = 1.0;
	fXform.dx = 0;
	fXform.dy = 0;

	// The 1.0f could be pretty bad here since it's not accounting for DPI, every reference in gecko uses 1.0
	HRESULT hr = mDwriteFactory->CreateGlyphRunAnalysis(&glyphRun, 1.0f, NULL, aRenderMode, aMeasureMode, 0.0f, 0.0f, &analysis);
	if (hr != S_OK) {
		printf("HResultis: %x\n", hr);
		assert(hr == S_OK);
	}

	RECT bounds;
	analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);

	float width = bounds.right - bounds.left;
	float height = bounds.bottom - bounds.top;
	int bufferSize = width * height * 3;
	BYTE* image = (BYTE*)malloc(bufferSize);
	memset(image, 0xFF, bufferSize);

	hr = analysis->CreateAlphaTexture(DWRITE_TEXTURE_TYPE::DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, image, bufferSize);
	assert(hr == S_OK);

	/*
	printf("Alpha texture\n");
	for (int i = 0; i < (int)height; i++) {
		for (int j = 0; j < (int)width * 3; j += 3) {
			int row = i * width;
			printf("(%u,%u,%u) ",
				image[row + j],
				image[row + j + 1],
				image[row + j + 2]
			);
		}
		printf("\n");
	}
	*/

	BYTE* bitmapImage = ConvertToRGBA(image, width, height, useLUT, convert, useGDILUT);

	/*
	for (int i = 0; i < (int)height; i++) {
		for (int j = 0; j < (int)width * 3; j += 3) {
			int row = i * width;
			printf("(%u,%u,%u) ",
				image[row + j],
				image[row + j + 1],
				image[row + j + 2]
			);
		}
		printf("\n\n");
	}
	*/

	// Now try to make a bitmap
	ID2D1Bitmap* bitmap;
	D2D1_BITMAP_PROPERTIES properties = { DXGI_FORMAT_R8G8B8A8_UNORM,  D2D1_ALPHA_MODE_PREMULTIPLIED };
	D2D1_SIZE_U size = D2D1::SizeU(width, height);

	hr = mRenderTarget->CreateBitmap(size, bitmapImage, width * 4, properties, &bitmap);
	assert(hr == S_OK);

	// Finally draw the bitmap somewhere
	D2D1_RECT_F destRect;
	destRect.left = x + bounds.left;
	destRect.right = x + bounds.right;
	destRect.top = y + bounds.top;
	destRect.bottom = y + bounds.bottom;

	mRenderTarget->DrawBitmap(bitmap, &destRect, 1.0);
	mRenderTarget->EndDraw();
}

void D2DSetup::AlternateText(int count) {
	IDWriteFontFace* fontFace = GetFontFace();
	int x = 100; int y = 100;
	switch (count % 2) {
	case 0:
	{
		WCHAR d2dMessage[] = L"The Donald Trump GDI";
		DWRITE_GLYPH_RUN d2dGlyphRun;
		CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
		DrawTextWithD2D(d2dGlyphRun, x, y, mGDIParams, true);
		break;
	}
	case 1:
	{
		/*
		WCHAR d2dMessage[] = L"Donald Trump Sucks Custom";
		DWRITE_GLYPH_RUN d2dGlyphRun;
		CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
		DrawTextWithD2D(d2dGlyphRun, x, y, mCustomParams, true);
		break;
		*/
		WCHAR d2dLutChop[] = L"The Donald Trump GDI LUT";
		DWRITE_GLYPH_RUN d2dLutChopRun;
		CreateGlyphRunAnalysis(d2dLutChopRun, fontFace, d2dLutChop);
		DrawWithBitmap(d2dLutChopRun, x, y, true, true,
						DWRITE_RENDERING_MODE_GDI_CLASSIC,
						DWRITE_MEASURING_MODE_NATURAL,
						true, true);
		break;
	}
	} // end switch
}

void D2DSetup::DrawWithMask()
{
	IDWriteFontFace* fontFace = GetFontFace();
	const int x = 100;
	const int y = 100;

	DWRITE_RENDERING_MODE recommendedMode = DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL;
	HRESULT hr = fontFace->GetRecommendedRenderingMode(mFontSize,
		1.0f,
		DWRITE_MEASURING_MODE_NATURAL, // We use this in gecko
		mDefaultParams,
		&recommendedMode);
	assert(hr == S_OK);
	printf("Recommended mode is: %d\n", recommendedMode);

	/*
	WCHAR d2dMessage[] = L"T";
	DWRITE_GLYPH_RUN d2dGlyphRun;
	CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
	DrawTextWithD2D(d2dGlyphRun, x, y, mDefaultParams);
	*/

	WCHAR d2dMessage[] = L"The Donald Trump Sucks GDI";
	DWRITE_GLYPH_RUN d2dGlyphRun;
	CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
	DrawTextWithD2D(d2dGlyphRun, x, y, mGDIParams);

	WCHAR sym[] = L"The Donald Trump Sucks LUT";
	DWRITE_GLYPH_RUN symRun;
	CreateGlyphRunAnalysis(symRun, fontFace, sym);
	DrawWithBitmap(symRun, x, y + 20, true, true, DWRITE_RENDERING_MODE_GDI_CLASSIC);

	WCHAR gdi[] = L"The Donald Trump Sucks LUT GDI";
	DWRITE_GLYPH_RUN gdiRun;
	CreateGlyphRunAnalysis(gdiRun, fontFace, gdi);
	DrawWithBitmap(gdiRun, x, y + 40, true, true,
					DWRITE_RENDERING_MODE_GDI_CLASSIC,
					DWRITE_MEASURING_MODE_NATURAL,
					false,
					true);
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

	mFontSize = 13;
	printf("Font size is: %d\n", mFontSize);
	hr = mDwriteFactory->CreateTextFormat(L"Arial", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, mFontSize, L"", &mTextFormat);
	assert(hr == S_OK);

	mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(0x404040, 1.0f), &mDarkBlackBrush);
	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &mBlackBrush);
	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 1.0f), &mWhiteBrush);
	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.0f), &mTransparentBlackBrush);

	// Now we can play with params
	mRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_DEFAULT);

	IDWriteRenderingParams* defaultParams;
	mDwriteFactory->CreateRenderingParams(&mDefaultParams);

	IDWriteRenderingParams* customParams;
	printf("Default param contrast is: %f, gamma: %f, render mode: %d, pixel geometry %d\n",
			mDefaultParams->GetEnhancedContrast(),
			mDefaultParams->GetGamma(),
			mDefaultParams->GetRenderingMode(),
			mDefaultParams->GetPixelGeometry());

	float contrast = mDefaultParams->GetEnhancedContrast();
	contrast = 1.0f;

	printf("Default rendering modeis: %d\n", DWRITE_RENDERING_MODE_DEFAULT);

	mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, mDefaultParams->GetClearTypeLevel(), mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_DEFAULT, &mCustomParams);
	printf("Contrast: %f, gamma: %f, cleartype level: %f, rendering mode: %d\n",
			mCustomParams->GetEnhancedContrast(),
			mCustomParams->GetGamma(),
			mCustomParams->GetClearTypeLevel(),
			mCustomParams->GetRenderingMode());

	DWRITE_PIXEL_GEOMETRY geometry = mDefaultParams->GetPixelGeometry();
	mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, mDefaultParams->GetClearTypeLevel(), mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_GDI_CLASSIC, &mGDIParams);

	IDWriteRenderingParams* monitorParams;
	mDwriteFactory->CreateMonitorRenderingParams(GetPrimaryMonitorHandle(), &monitorParams);

	float dpiX = 0;
	float dpiY = 0;
	mRenderTarget->GetDpi(&dpiX, &dpiY);
	printf("DPI x: %f, y: %f\n", dpiX, dpiY);
}

SkMaskGamma::PreBlend D2DSetup::CreateLUT()
{
	//const float contrast = 0.5;
	const float contrast = 1.0;
	const float paintGamma = 1.8;
	const float deviceGamma = 1.8;
	SkMaskGamma* gamma = new SkMaskGamma(contrast, paintGamma, deviceGamma);

	// Gecko is always setting the preblend to black background.
	SkColor blackLuminanceColor = SkColorSetARGBInline(255, 0, 0, 0);
	SkColor whiteLuminance = SkColorSetARGBInline(255, 255, 255, 255);
	SkColor mozillaColor = SkColorSetARGBInline(255, 0x40, 0x40, 0x40);
	return gamma->preBlend(blackLuminanceColor);
}

// See http://searchfox.org/mozilla-central/source/gfx/skia/skia/src/ports/SkFontHost_win.cpp#1080
SkMaskGamma::PreBlend D2DSetup::CreateGdiLUT()
{
	UINT level = 0;
	if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &level, 0) || !level) {
		// can't get the data, so use a default
		level = 1400;
	}

	const float contrast = 1.0;
	//const float paintGamma = (float) level / 1000.0f;
	//const float deviceGamma = (float)level / 1000.0f;

	const float paintGamma = 2.3f;
	const float deviceGamma = 2.3f;
	SkMaskGamma* gamma = new SkMaskGamma(contrast, paintGamma, deviceGamma);

	// Gecko is always setting the preblend to black background.
	SkColor blackLuminanceColor = SkColorSetARGBInline(255, 0, 0, 0);
	SkColor whiteLuminance = SkColorSetARGBInline(255, 255, 255, 255);
	SkColor mozillaColor = SkColorSetARGBInline(255, 0x40, 0x40, 0x40);
	return gamma->preBlend(blackLuminanceColor);
}