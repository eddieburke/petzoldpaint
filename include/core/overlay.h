#ifndef OVERLAY_H
#define OVERLAY_H
#include <windows.h>
#include "geom.h"

typedef struct {
    HDC hdc;
    Viewport vp;
} OverlayContext;

#define OVERLAY_HANDLE_SQUARE  0
#define OVERLAY_HANDLE_CIRCLE  1

void Overlay_Init(OverlayContext* ctx, HDC hdc, double dScale, int nDestX, int nDestY);
void Overlay_BmpToScr(const OverlayContext* ctx, int xBmp, int yBmp, int* xScr, int* yScr);
void Overlay_DrawHandle(const OverlayContext* ctx, int xBmp, int yBmp, int type, BOOL bHollow);
void Overlay_DrawBoxHandles(const OverlayContext* ctx, const RECT* rcBitmap);
void Overlay_DrawPolyHandles(const OverlayContext* ctx, const POINT* ptsBitmap, int count);
void Overlay_DrawSelectionFrame(const OverlayContext* ctx, const RECT* rcBitmap, BOOL bDotted);
void Overlay_DrawLine(const OverlayContext* ctx, int x1Bmp, int y1Bmp, int x2Bmp, int y2Bmp,
                      COLORREF color, int penStyle);
void Overlay_DrawPolyline(const OverlayContext* ctx, const POINT* ptsBitmap, int count,
                          COLORREF color, int penStyle);
int Overlay_HitTestBoxHandles(const RECT* rcBmp, int xBmp, int yBmp);

#endif
