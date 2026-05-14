/*------------------------------------------------------------
    gdi_utils.h - GDI, bitmap, font, and preview buffer utilities
------------------------------------------------------------*/

#ifndef GDI_UTILS_H
#define GDI_UTILS_H

#include <windows.h>

/*------------------------------------------------------------
    GDI Resource Management
------------------------------------------------------------*/

HDC GetScreenDC(void);
void ReleaseScreenDC(HDC hdc);
HDC CreateTempDC(HDC hdcRef);
void DeleteTempDC(HDC hdc);
HPEN CreatePenAndSelect(HDC hdc, int style, int width, COLORREF color,
                        HPEN *phOld);
void RestorePen(HDC hdc, HPEN hOld);
void Gdi_DeletePen(HPEN hPen);
HBRUSH CreateBrushAndSelect(HDC hdc, COLORREF color, HBRUSH *phOld);
void RestoreBrush(HDC hdc, HBRUSH hOld);
void Gdi_DeleteBrush(HBRUSH hBrush);
void Gdi_DeleteFont(HFONT hFont);

HDC GetCanvasBitmapDC(HBITMAP *phOld);
void ReleaseCanvasBitmapDC(HDC hdc, HBITMAP hOld);
HDC GetBitmapDC(HBITMAP hBmp, HBITMAP *phOld);
void ReleaseBitmapDC(HDC hdc, HBITMAP hOld);

/*------------------------------------------------------------
    Bitmap Utilities
------------------------------------------------------------*/

HBITMAP CreateDibSection32(int width, int height, BYTE **outBits);
HBITMAP CopyBitmapToDib32(HBITMAP hSrc, int w, int h, BYTE **outBits);

/*------------------------------------------------------------
    Bitmap Transformation
------------------------------------------------------------*/

typedef void (*BitmapTransformFunc)(HDC hSrcDC, HDC hDstDC, int nWidth,
                                    int nHeight, void *pUserData);
HBITMAP TransformBitmap(HBITMAP hSrc, int nSrcW, int nSrcH, int nDstW,
                        int nDstH, BitmapTransformFunc pfnTransform,
                        void *pUserData);

#include "pixel_ops.h"
#define PremultiplyAlpha(bits, count) PixelOps_Premultiply((bits), (count), 1)
#define Transform_Flip PixelOps_Flip

/*------------------------------------------------------------
    Font and Text
------------------------------------------------------------*/

HFONT CreateClearTypeFont(int size, int weight, BOOL bItalic,
                          const char *faceName);
HFONT CreateSegoiUIFont(int size, int weight);
void SetupTextRender(HDC hdc, COLORREF color);

/*------------------------------------------------------------
    Preview Buffer Helpers
------------------------------------------------------------*/

BOOL GetDibBitsFromHdc(HDC hdc, BYTE **outBits, int *outWidth, int *outHeight);
BOOL GetPreviewBufferBits(HDC hdc, BYTE **outBits, int *outWidth,
                          int *outHeight);

#endif /* GDI_UTILS_H */
