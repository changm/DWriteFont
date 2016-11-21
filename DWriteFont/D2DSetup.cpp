
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
	mDefaultParams->Release();
	mCustomParams->Release();

}

void D2DSetup::Clear()
{
	mRenderTarget->BeginDraw();
	mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
	mRenderTarget->EndDraw();
}

void D2DSetup::DrawText()
{
	static const WCHAR message[] = L"Hello World DrawText";
	mRenderTarget->BeginDraw();

	mRenderTarget->DrawTextW(message,
		ARRAYSIZE(message) - 1,
		mTextFormat,
		D2D1::RectF(0, 100, 500, 500),
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

IDWriteFontFace* D2DSetup::GetFontFace()
{
	static const WCHAR fontFamilyName[] = L"Arial";

	IDWriteFontCollection* systemFonts;
	mDwriteFactory->GetSystemFontCollection(&systemFonts, TRUE);
	PrintFonts(systemFonts);

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

void D2DSetup::DrawTextWithD2D(DWRITE_GLYPH_RUN& glyphRun, int x, int y, IDWriteRenderingParams* aParams)
{
	D2D1_POINT_2F origin;
	origin.x = x;
	origin.y = y;

	mRenderTarget->BeginDraw();
	mRenderTarget->SetTextRenderingParams(aParams);
	mRenderTarget->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	mRenderTarget->DrawGlyphRun(origin, &glyphRun, mBlackBrush);
	mRenderTarget->EndDraw();
}

// Converts the given rgb 3x1 cleartype alpha mask to the required RGBA_UNOM as required by bitmaps
// Also blends to draw black text on white
BYTE* D2DSetup::ConvertToRGBA(BYTE* aRGB, int width, int height, bool useLUT, bool convert)
{
	int size = width * height * 4;
	BYTE* bitmapImage = (BYTE*)malloc(size);
	memset(bitmapImage, 0x00, size);

	const uint8_t* tableR = this->fPreBlend.fR;
	const uint8_t* tableG = this->fPreBlend.fG;
	const uint8_t* tableB = this->fPreBlend.fB;

	for (int y = 0; y < height; y++) {
		int sourceHeight = y * width * 3;	// expect 3 bytes per pixel
		int destHeight = y * width * 4;		// convert to 4 bytes per pixel to add alpha opaque channel

		for (int i = 0; i < width; i++) {
			int destIndex = destHeight + (4 * i);
			int srcIndex = sourceHeight + (i * 3);

			BYTE r = aRGB[srcIndex];
			BYTE g = aRGB[srcIndex + 1];
			BYTE b = aRGB[srcIndex + 2];

			if (useLUT) {
				r = sk_apply_lut_if<true>(r, tableR);
				g = sk_apply_lut_if<true>(g, tableG);
				b = sk_apply_lut_if<true>(b, tableB);
			}

			if (convert) {
				// Taken from http://searchfox.org/mozilla-central/source/gfx/skia/skia/include/core/SkColorPriv.h#654
				BYTE shortR = r >> 3;
				BYTE shortG = g >> 2;
				BYTE shortB = b >> 3;

				BYTE oldR = r;

				r = shortR << 3;
				g = shortG << 2;
				b = shortB << 3;
			}

			// Blend to draw black text on white
			// Mozilla's color is 0x404040
			r = Blend(0x00, 0xFF, r);
			g = Blend(0x00, 0xFF, g);
			b = Blend(0x00, 0xFF, b);

			bitmapImage[destIndex] = r;
			bitmapImage[destIndex + 1] = g;
			bitmapImage[destIndex + 2] = b;
			bitmapImage[destIndex + 3] = 0xFF;
		}
	}
	return bitmapImage;
}

void D2DSetup::DrawWithBitmap(DWRITE_GLYPH_RUN& glyphRun, int x, int y, bool useLUT, bool convert)
{
	mRenderTarget->BeginDraw();

	IDWriteGlyphRunAnalysis* analysis;
	// The 1.0f could be pretty bad here since it's not accounting for DPI, every reference in gecko uses 1.0
	mDwriteFactory->CreateGlyphRunAnalysis(&glyphRun, 1.0f, NULL, DWRITE_RENDERING_MODE_CLEARTYPE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0.0f, 0.0f, &analysis);

	RECT bounds;
	analysis->GetAlphaTextureBounds(DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds);

	float width = bounds.right - bounds.left;
	float height = bounds.bottom - bounds.top;
	int bufferSize = width * height * 3;
	BYTE* image = (BYTE*)malloc(bufferSize);
	memset(image, 0xFF, bufferSize);
	HRESULT hr = analysis->CreateAlphaTexture(DWRITE_TEXTURE_TYPE::DWRITE_TEXTURE_CLEARTYPE_3x1, &bounds, image, bufferSize);
	assert(hr == S_OK);

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

	BYTE* bitmapImage = ConvertToRGBA(image, width, height, useLUT, convert);

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

void D2DSetup::DrawWithMask()
{
	IDWriteFontFace* fontFace = GetFontFace();
	const int xAxis = 0;
	const int yAxis = 100;

	/*
	WCHAR bitmapMessage[] = L"Hello World Bitmap";
	DWRITE_GLYPH_RUN bitmapGlyphRun;
	CreateGlyphRunAnalysis(bitmapGlyphRun, fontFace, bitmapMessage);
	DrawWithBitmap(bitmapGlyphRun, xAxis, yAxis, false);
	*/

	WCHAR bitmapMessage[] = L"Hello World LUT Trim";
	DWRITE_GLYPH_RUN bitmapGlyphRun;
	CreateGlyphRunAnalysis(bitmapGlyphRun, fontFace, bitmapMessage);
	DrawWithBitmap(bitmapGlyphRun, xAxis, yAxis, true, true);

	WCHAR lutMessage[] = L"Hello World LUT";
	DWRITE_GLYPH_RUN lutGlyphRun;
	CreateGlyphRunAnalysis(lutGlyphRun, fontFace, lutMessage);
	DrawWithBitmap(lutGlyphRun, xAxis, yAxis - 20, true);

	WCHAR d2dMessage[] = L"Hello World D2D";
	DWRITE_GLYPH_RUN d2dGlyphRun;
	CreateGlyphRunAnalysis(d2dGlyphRun, fontFace, d2dMessage);
	DrawTextWithD2D(d2dGlyphRun, xAxis, yAxis - 40, mDefaultParams);
}

void D2DSetup::DrawSkia()
{
	int x = 100;
	int y = 100;

	IDWriteFontFace* fontFace = GetFontFace();

	WCHAR message[] = L"Hello World";
	DWRITE_GLYPH_RUN glyphRun;
	CreateGlyphRunAnalysis(glyphRun, fontFace, message);


	//DrawWithBitmap(glyphRun, x, y - 60, true, true);
	//DrawWithBitmap(glyphRun, x, y - 40, true);
	DrawWithBitmap(glyphRun, x, y - 20, false);
	//DrawTextWithD2D(glyphRun, x, y, mDefaultParams);

	/*
	mRenderTarget->BeginDraw();
	mRenderTarget->DrawTextW(message,
		ARRAYSIZE(message) - 1,
		mTextFormat,
		D2D1::RectF(x, y, 100, 100),
		mBlackBrush);
	mRenderTarget->EndDraw();
	*/
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

	mFontSize = 14;
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
	printf("Default param contrast is: %f, gamma: %f\n",
		    mDefaultParams->GetEnhancedContrast(),
			mDefaultParams->GetGamma());
	float contrast = mDefaultParams->GetEnhancedContrast();
	contrast = 1.0f;

	mDwriteFactory->CreateCustomRenderingParams(mDefaultParams->GetGamma(), contrast, mDefaultParams->GetClearTypeLevel(), mDefaultParams->GetPixelGeometry(), DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, &mCustomParams);
	printf("Custom params is: %f\n", mCustomParams->GetEnhancedContrast());

	IDWriteRenderingParams* monitorParams;
	mDwriteFactory->CreateMonitorRenderingParams(GetPrimaryMonitorHandle(), &monitorParams);

	float dpiX = 0;
	float dpiY = 0;
	mRenderTarget->GetDpi(&dpiX, &dpiY);
	printf("DPI x: %f, y: %f\n", dpiX, dpiY);
}

SkMaskGamma::PreBlend D2DSetup::CreateLUT()
{
	const float contrast = 0.5;
	const float paintGamma = 1.8;
	const float deviceGamma = 1.8;
	SkMaskGamma* gamma = new SkMaskGamma(contrast, paintGamma, deviceGamma);

	// Gecko is always setting the preblend to black background.
	SkColor blackLuminanceColor = SkColorSetARGBInline(255, 0, 0, 0);
	SkColor whiteLuminance = SkColorSetARGBInline(255, 255, 255, 255);
	SkColor mozillaColor = SkColorSetARGBInline(255, 0x40, 0x40, 0x40);
	return gamma->preBlend(blackLuminanceColor);
}