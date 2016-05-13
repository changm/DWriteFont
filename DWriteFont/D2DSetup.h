#pragma once

#include <dwrite.h>
#include <direct.h>
#include <d3d11.h>
#include <d2d1.h>

class D2DSetup
{
public:
	D2DSetup(HWND aHWND, HDC aHDC) {
		mHDC = aHDC;
		mHWND = aHWND;
		Init();
	}

	~D2DSetup();

	void DrawText();
	void Init();
	void DrawWithMask();
	void PrintFonts(IDWriteFontCollection* aFontCollection);

private:
	HWND mHWND;
	HDC mHDC;

	ID2D1Factory*	mFactory;
	ID2D1HwndRenderTarget*	mRenderTarget;
	IDWriteFactory* mDwriteFactory;
	IDWriteTextFormat* mTextFormat;
	ID2D1SolidColorBrush* mBlackBrush;
	ID2D1SolidColorBrush* mWhiteBrush;
	ID2D1SolidColorBrush* mTransparentBlackBrush;

	IDWriteRenderingParams* mCustomParams;
	IDWriteRenderingParams* mDefaultParams;
};
