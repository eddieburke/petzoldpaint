/*------------------------------------------------------------
    gdi_utils.c - GDI, bitmap, font, and preview buffer utilities
------------------------------------------------------------*/

#include "gdi_utils.h"
#include "layers.h"

/*------------------------------------------------------------
    GDI Resource Management
------------------------------------------------------------*/

HDC GetScreenDC(void) { return GetDC(NULL); }

void ReleaseScreenDC(HDC hdc) { ReleaseDC(NULL, hdc); }

HDC CreateTempDC(HDC hdcRef) { return CreateCompatibleDC(hdcRef); }

void DeleteTempDC(HDC hdc) { DeleteDC(hdc); }

HPEN CreatePenAndSelect(HDC hdc, int style, int width, COLORREF color,
                        HPEN *phOld) {
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

HBRUSH CreateBrushAndSelect(HDC hdc, COLORREF color, HBRUSH *phOld) {
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

/*------------------------------------------------------------
    Bitmap Utilities
------------------------------------------------------------*/

HBITMAP CreateDibSection32(int width, int height, BYTE **outBits) {
  BITMAPINFO bmi;
  HDC hdcScreen;
  HBITMAP hBmp;

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
  hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void **)outBits,
                          NULL, 0);
  ReleaseScreenDC(hdcScreen);

  return hBmp;
}

HBITMAP CopyBitmapToDib32(HBITMAP hSrc, int w, int h, BYTE **outBits) {
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

/*------------------------------------------------------------
    Bitmap Transformation
------------------------------------------------------------*/

HBITMAP TransformBitmap(HBITMAP hSrc, int nSrcW, int nSrcH, int nDstW,
                        int nDstH, BitmapTransformFunc pfnTransform,
                        void *pUserData) {
  if (!hSrc || !pfnTransform || nDstW <= 0 || nDstH <= 0)
    return NULL;

  HDC hdcScreen = GetScreenDC();
  if (!hdcScreen)
    return NULL;

  HDC hSrcDC = CreateTempDC(hdcScreen);
  HDC hDstDC = CreateTempDC(hdcScreen);
  HBITMAP hDstBmp = CreateCompatibleBitmap(hdcScreen, nDstW, nDstH);

  if (!hSrcDC || !hDstDC || !hDstBmp) {
    if (hSrcDC)
      DeleteTempDC(hSrcDC);
    if (hDstDC)
      DeleteTempDC(hDstDC);
    if (hDstBmp)
      DeleteObject(hDstBmp);
    ReleaseScreenDC(hdcScreen);
    return NULL;
  }

  HBITMAP hOldSrc = (HBITMAP)SelectObject(hSrcDC, hSrc);
  HBITMAP hOldDst = (HBITMAP)SelectObject(hDstDC, hDstBmp);

  pfnTransform(hSrcDC, hDstDC, nSrcW, nSrcH, pUserData);

  SelectObject(hSrcDC, hOldSrc);
  SelectObject(hDstDC, hOldDst);
  DeleteTempDC(hSrcDC);
  DeleteTempDC(hDstDC);
  ReleaseScreenDC(hdcScreen);

  return hDstBmp;
}

/*------------------------------------------------------------
    Font and Text
------------------------------------------------------------*/

HFONT CreateClearTypeFont(int size, int weight, BOOL bItalic,
                          const char *faceName) {
  return CreateFont(size, 0, 0, 0, weight, bItalic, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, faceName);
}

HFONT CreateSegoiUIFont(int size, int weight) {
  return CreateClearTypeFont(size, weight, FALSE, "Segoe UI");
}

void SetupTextRender(HDC hdc, COLORREF color) {
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, color);
}
