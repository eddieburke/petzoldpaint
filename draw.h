/*------------------------------------------------------------
    draw.h - Handle drawing, themed UI, and pixel primitives
------------------------------------------------------------*/

#ifndef DRAW_H
#define DRAW_H

#include <windows.h>

/*------------------------------------------------------------
    Handle Drawing Constants
------------------------------------------------------------*/

#define H_SIZE 5
#define H_GRAB 5

#define HT_NONE -1
#define HT_TL 0
#define HT_T 1
#define HT_TR 2
#define HT_R 3
#define HT_BR 4
#define HT_B 5
#define HT_BL 6
#define HT_L 7
#define HT_BODY 100
#define HT_ROTATE_TL 10
#define HT_ROTATE_TR 11
#define HT_ROTATE_BR 12
#define HT_ROTATE_BL 13

void DrawSelectionFrame(HDC hdc, RECT *rc, BOOL bDotted);
int HitTestBoxHandles(RECT *rc, int x, int y);
int HitTestBoxHandlesEx(RECT *rc, int x, int y, int tolerance);
void ResizeRect(RECT *rc, int handle, int dx, int dy, int minSize,
                BOOL bKeepAspect);
LPCTSTR GetHandleCursor(int handleId);

/*------------------------------------------------------------
    Themed Drawing
------------------------------------------------------------*/

BOOL IsThemeEnabled(void);
BOOL DrawThemedBackground(HDC hdc, LPCWSTR className, int part, int state,
                          RECT *prc);
void DrawThemedButtonState(HDC hdc, RECT *prc, int state);
void DrawThemedButton(HDC hdc, RECT *prc, BOOL bPressed);
void ClearClientRect(HDC hdc, HWND hwnd, HBRUSH hBrush);

/*------------------------------------------------------------
    Drawing Primitives
------------------------------------------------------------*/

/* Drawing Primitives with Alpha and Blend Mode */
void DrawPixelAlpha(BYTE *bits, int width, int height, int x, int y,
                    COLORREF color, BYTE alpha, int mode);

/* Integer Bresenham spine (8-connected); invokes fn once per pixel including ends. */
typedef void (*DrawLineSpineFn)(BYTE *bits, int width, int height, int x, int y,
                                void *userData);
void DrawLineSpineEach(BYTE *bits, int width, int height, int x1, int y1, int x2,
                       int y2, DrawLineSpineFn fn, void *userData);

/* Float segment → integer Bresenham spine with axis-aligned thickness stamp. */
void DrawLineStampCircles(BYTE *bits, int width, int height, float x1, float y1,
                          float x2, float y2, float radius, COLORREF color,
                          BYTE alpha, int mode);

/* Axis-aligned stamp per spine pixel; thickness supports even widths (2×2, …). */
void DrawLineAlpha(BYTE *bits, int width, int height, int x1, int y1, int x2,
                   int y2, int thickness, COLORREF color, BYTE alpha,
                   int mode);
void DrawCircleAlpha(BYTE *bits, int width, int height, int x, int y,
                     int radius, COLORREF color, BYTE alpha, int mode);

void DrawRectAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                   int h, COLORREF color, BYTE alpha, int mode);
void DrawRectOutlineAlpha(BYTE *bits, int width, int height, int x, int y,
                          int w, int h, COLORREF color, BYTE alpha,
                          int thickness, int mode);

void EraseRectAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                    int h);

void DrawEllipseAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                      int h, COLORREF color, BYTE alpha, BOOL bFill,
                      int thickness, int mode);

/* Rounded rect via signed-distance field; outline is band -thickness <= d <= 0. */
void DrawRoundedRectAlpha(BYTE *bits, int width, int height, int x, int y,
                          int w, int h, int radius, COLORREF color, BYTE alpha,
                          BOOL bFill, int thickness, int mode);

/*------------------------------------------------------------
    Compositing and Fill
------------------------------------------------------------*/

#include "pixel_ops.h"

#define FillCheckerboard PixelOps_FillCheckerboard
#define FillSolidAlpha PixelOps_Fill
#define BlendMultiply PixelOps_ApplyBlendMultiply
#define BlendScreen PixelOps_ApplyBlendScreen
#define BlendOverlay PixelOps_ApplyBlendOverlay
 
#endif /* DRAW_H */
