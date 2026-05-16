#ifndef DRAW_H
#define DRAW_H
#include <windows.h>
#include "geom.h"
void DrawSelectionFrame(HDC hdc, RECT *rc, BOOL bDotted);
void ResizeRect(RECT *rc, int handle, int dx, int dy, int minSize, BOOL bKeepAspect);
LPCTSTR
GetHandleCursor(int handleId);
BOOL IsThemeEnabled(void);
BOOL DrawThemedBackground(HDC hdc, LPCWSTR className, int part, int state, RECT *prc);
void DrawThemedButtonState(HDC hdc, RECT *prc, int state);
void DrawThemedButton(HDC hdc, RECT *prc, BOOL bPressed);
void ClearClientRect(HDC hdc, HWND hwnd, HBRUSH hBrush);
void DrawPixelAlpha(BYTE *bits, int width, int height, int x, int y, COLORREF color, BYTE alpha, int mode);
typedef void (*DrawLineSpineFn)(BYTE *bits, int width, int height, int x, int y, void *userData);
void DrawLineSpineEach(BYTE *bits, int width, int height, int x1, int y1, int x2, int y2, DrawLineSpineFn fn, void *userData);
void DrawLineStampCircles(BYTE *bits, int width, int height, float x1, float y1, float x2, float y2, float radius, COLORREF color, BYTE alpha, int mode);
void DrawLineAlpha(BYTE *bits, int width, int height, int x1, int y1, int x2, int y2, int thickness, COLORREF color, BYTE alpha, int mode);
void DrawCircleAlpha(BYTE *bits, int width, int height, int x, int y, int radius, COLORREF color, BYTE alpha, int mode);
void DrawRectAlpha(BYTE *bits, int width, int height, int x, int y, int w, int h, COLORREF color, BYTE alpha, int mode);
void DrawRectOutlineAlpha(BYTE *bits, int width, int height, int x, int y, int w, int h, COLORREF color, BYTE alpha, int thickness, int mode);
void EraseRectAlpha(BYTE *bits, int width, int height, int x, int y, int w, int h);
void DrawEllipseAlpha(BYTE *bits, int width, int height, int x, int y, int w, int h, COLORREF color, BYTE alpha, BOOL bFill, int thickness, int mode);
void DrawRoundedRectAlpha(BYTE *bits, int width, int height, int x, int y, int w, int h, int radius, COLORREF color, BYTE alpha, BOOL bFill, int thickness, int mode);
#include "pixel_ops.h"
#define FillCheckerboard PixelOps_FillCheckerboard
#define FillSolidAlpha   PixelOps_Fill
#define BlendMultiply    PixelOps_ApplyBlendMultiply
#define BlendScreen      PixelOps_ApplyBlendScreen
#define BlendOverlay     PixelOps_ApplyBlendOverlay
#endif
