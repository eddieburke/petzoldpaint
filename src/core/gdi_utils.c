#include "gdi_utils.h"
#include "layers.h"
HDC GetScreenDC(void) {
	return GetDC(NULL);
}
void ReleaseScreenDC(HDC hdc) {
	ReleaseDC(NULL, hdc);
}
HDC CreateTempDC(HDC hdcRef) {
	return CreateCompatibleDC(hdcRef);
}
void DeleteTempDC(HDC hdc) {
	DeleteDC(hdc);
}
BOOL Gdi_SelectObject(HDC hdc, HGDIOBJ obj, GdiSelection *sel) {
	HGDIOBJ
	previous;
	if (!sel)
		return FALSE;
	sel->hdc = NULL;
	sel->previous = NULL;
	sel->selected = NULL;
	if (!hdc || !obj)
		return FALSE;
	previous = SelectObject(hdc, obj);
	if (!previous || previous == (HGDIOBJ)HGDI_ERROR)
		return FALSE;
	sel->hdc = hdc;
	sel->previous = previous;
	sel->selected = obj;
	return TRUE;
}
void Gdi_RestoreSelection(GdiSelection *sel) {
	if (sel && sel->hdc && sel->previous) {
		SelectObject(sel->hdc, sel->previous);
		sel->hdc = NULL;
		sel->previous = NULL;
		sel->selected = NULL;
	}
}
HPEN CreatePenAndSelect(HDC hdc, int style, int width, COLORREF color, HPEN *phOld) {
	HPEN hPen = CreatePen(style, width, color);
	if (hPen && phOld) {
		*phOld = (HPEN)SelectObject(hdc, hPen);
	}
	return hPen;
}
void RestorePen(HDC hdc, HPEN hOld) {
	if (hOld) {
		SelectObject(hdc, hOld);
	}
}
void Gdi_DeletePen(HPEN hPen) {
	if (hPen) {
		DeleteObject(hPen);
	}
}
HBRUSH
CreateBrushAndSelect(HDC hdc, COLORREF color, HBRUSH *phOld) {
	HBRUSH hBrush = CreateSolidBrush(color);
	if (hBrush && phOld) {
		*phOld = (HBRUSH)SelectObject(hdc, hBrush);
	}
	return hBrush;
}
void RestoreBrush(HDC hdc, HBRUSH hOld) {
	if (hOld) {
		SelectObject(hdc, hOld);
	}
}
void Gdi_DeleteBrush(HBRUSH hBrush) {
	if (hBrush) {
		DeleteObject(hBrush);
	}
}
void Gdi_DeleteFont(HFONT hFont) {
	if (hFont) {
		DeleteObject(hFont);
	}
}
HDC GetCanvasBitmapDC(HBITMAP *phOld) {
	HBITMAP hBitmap = LayersGetActiveColorBitmap();
	if (phOld)
		*phOld = NULL;
	if (!hBitmap)
		return NULL;
	HDC hdcScreen = GetScreenDC();
	if (!hdcScreen)
		return NULL;
	HDC hMemDC = CreateTempDC(hdcScreen);
	if (!hMemDC) {
		ReleaseScreenDC(hdcScreen);
		return NULL;
	}
	HBITMAP hPrev = (HBITMAP)SelectObject(hMemDC, hBitmap);
	if (!hPrev || hPrev == (HBITMAP)(HGDIOBJ)-1) {
		DeleteTempDC(hMemDC);
		ReleaseScreenDC(hdcScreen);
		return NULL;
	}
	if (phOld)
		*phOld = hPrev;
	ReleaseScreenDC(hdcScreen);
	return hMemDC;
}
void ReleaseCanvasBitmapDC(HDC hdc, HBITMAP hOld) {
	if (hdc) {
		if (hOld) {
			SelectObject(hdc, hOld);
		}
		DeleteTempDC(hdc);
	}
}
HDC GetBitmapDC(HBITMAP hBmp, HBITMAP *phOld) {
	if (phOld)
		*phOld = NULL;
	if (!hBmp)
		return NULL;
	HDC hdcScreen = GetScreenDC();
	if (!hdcScreen)
		return NULL;
	HDC hMemDC = CreateTempDC(hdcScreen);
	if (!hMemDC) {
		ReleaseScreenDC(hdcScreen);
		return NULL;
	}
	HBITMAP hPrev = (HBITMAP)SelectObject(hMemDC, hBmp);
	if (!hPrev || hPrev == (HBITMAP)(HGDIOBJ)-1) {
		DeleteTempDC(hMemDC);
		ReleaseScreenDC(hdcScreen);
		return NULL;
	}
	if (phOld)
		*phOld = hPrev;
	ReleaseScreenDC(hdcScreen);
	return hMemDC;
}
HBITMAP
CreateDibSection32(int width, int height, BYTE **outBits) {
	BITMAPINFO
	bmi;
	HDC hdcScreen;
	HBITMAP
	hBmp;
	if (outBits)
		*outBits = NULL;
	if (width <= 0 || height <= 0)
		return NULL;
	ZeroMemory(&bmi, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = -height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	hdcScreen = GetScreenDC();
	hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void **)outBits, NULL, 0);
	ReleaseScreenDC(hdcScreen);
	return hBmp;
}
HBITMAP
CopyBitmapToDib32(HBITMAP hSrc, int w, int h, BYTE **outBits) {
	HDC hdc = GetScreenDC();
	BYTE *bits = NULL;
	HBITMAP hDib = CreateDibSection32(w, h, &bits);
	if (hDib && bits) {
		HDC hSrcDC = CreateTempDC(hdc);
		HDC hDstDC = CreateTempDC(hdc);
		HBITMAP hOldSrc = SelectObject(hSrcDC, hSrc);
		HBITMAP hOldDst = SelectObject(hDstDC, hDib);
		BitBlt(hDstDC, 0, 0, w, h, hSrcDC, 0, 0, SRCCOPY);
		SelectObject(hSrcDC, hOldSrc);
		SelectObject(hDstDC, hOldDst);
		DeleteTempDC(hSrcDC);
		DeleteTempDC(hDstDC);
		if (outBits)
			*outBits = bits;
	}
	ReleaseScreenDC(hdc);
	return hDib;
}
HFONT CreateClearTypeFont(int size, int weight, BOOL bItalic, const char *faceName) {
	return CreateFont(size, 0, 0, 0, weight, bItalic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, faceName);
}
HFONT CreateSegoiUIFont(int size, int weight) {
	return CreateClearTypeFont(size, weight, FALSE, "Segoe UI");
}
void SetupTextRender(HDC hdc, COLORREF color) {
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, color);
}
