
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