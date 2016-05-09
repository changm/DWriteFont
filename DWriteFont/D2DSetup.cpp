
#include "stdafx.h"
#include "D2DSetup.h"
#include <d2d1.h>
#include <d2d1helper.h>

D2DSetup::~D2DSetup()
{
	mFactory->Release();
	mRenderTarget->Release();
}

void D2DSetup::DrawText()
{
	mRenderTarget->BeginDraw();
	mRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Green));
	mRenderTarget->EndDraw();
}

void D2DSetup::Init()
{
	HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &mFactory);

	RECT clientRect;
	GetClientRect(mHWND, &clientRect);

	D2D1_SIZE_U size = D2D1::SizeU(
		clientRect.right - clientRect.left,
		clientRect.bottom - clientRect.top
	);

	// Create a Direct2D render target.
	const D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties();
	const D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProperties = D2D1::HwndRenderTargetProperties(mHWND, size);
	hr = mFactory->CreateHwndRenderTarget(&properties, &hwndProperties,
										  &mRenderTarget);
}