
#include "stdafx.h"
#include "D2DSetup.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <assert.h>

D2DSetup::~D2DSetup()
{
	mFactory->Release();
	mRenderTarget->Release();
	mDwriteFactory->Release();
	mTextFormat->Release();
	mBlackBrush->Release();
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

void D2DSetup::DrawWithMask()
{
	static const WCHAR message[] = L"H";
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
	UINT16 glyphIndices[1];
	UINT32 textLength = 1;
	UINT32 codePoints[1];
	codePoints[0] = message[0];
	fontFace->GetGlyphIndicesW(codePoints, textLength, glyphIndices);

	DWRITE_GLYPH_OFFSET offset;
	offset.advanceOffset = 0;
	offset.ascenderOffset = 0;

	DWRITE_GLYPH_RUN glyphRun;
	glyphRun.glyphCount = 1;
	glyphRun.glyphAdvances = &advance;
	glyphRun.fontFace = fontFace;
	glyphRun.fontEmSize = 28;
	glyphRun.bidiLevel = 0;
	glyphRun.glyphIndices = glyphIndices;
	glyphRun.isSideways = FALSE;
	glyphRun.glyphOffsets = &offset;

	IDWriteGlyphRunAnalysis* analysis;
	// The 1.0f could be pretty bad here since it's not accounting for DPI
	mDwriteFactory->CreateGlyphRunAnalysis(&glyphRun, 1.0f, NULL, DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, DWRITE_MEASURING_MODE_NATURAL, 0.f, 0.f, &analysis);

	D2D1_POINT_2F origin;
	origin.x = 100;
	origin.y = 100;
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

	hr = mDwriteFactory->CreateTextFormat(L"Helvetica", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24, L"", &mTextFormat);
	assert(hr == S_OK);

	mTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	mTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	hr = mRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 1.0f), &mBlackBrush);

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