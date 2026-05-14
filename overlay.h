#ifndef OVERLAY_H
#define OVERLAY_H
#include <windows.h>

/*
 * Overlay Drawing Module
 * 
 * This module provides unified helpers for drawing screen-space overlays
 * (handles, selection frames, marching ants, etc.) that need to remain
 * 1px sharp regardless of zoom level.
 * 
 * All coordinates passed to these functions are in BITMAP space.
 * The overlay context handles conversion to screen space internally.
 */

/* Overlay drawing context - set once per paint, reused for all draws */
typedef struct {
    HDC hdc;
    double dScale;
    int nDestX;
    int nDestY;
} OverlayContext;

/* Handle types for DrawHandle */
#define OVERLAY_HANDLE_SQUARE  0
#define OVERLAY_HANDLE_CIRCLE  1

/* Initialize overlay context with current viewport settings */
void Overlay_Init(OverlayContext* ctx, HDC hdc, double dScale, int nDestX, int nDestY);

/* Convert bitmap point to screen point (uses context) */
void Overlay_BmpToScr(const OverlayContext* ctx, int xBmp, int yBmp, int* xScr, int* yScr);

/* Draw a single handle at bitmap coordinates */
void Overlay_DrawHandle(const OverlayContext* ctx, int xBmp, int yBmp, int type, BOOL bHollow);

/* Draw 8 resize handles around a rectangle (bitmap coordinates) */
void Overlay_DrawBoxHandles(const OverlayContext* ctx, const RECT* rcBitmap);

/* Draw handles at each point in an array (bitmap coordinates) */
void Overlay_DrawPolyHandles(const OverlayContext* ctx, const POINT* ptsBitmap, int count);

/* Draw selection frame (marching ants or dashed) around rectangle */
void Overlay_DrawSelectionFrame(const OverlayContext* ctx, const RECT* rcBitmap, BOOL bDotted);

/* Draw a dotted/dashed rectangle outline */
void Overlay_DrawDottedRect(const OverlayContext* ctx, const RECT* rcBitmap);

/* Draw a line between two bitmap points */
void Overlay_DrawLine(const OverlayContext* ctx, int x1Bmp, int y1Bmp, int x2Bmp, int y2Bmp, 
                      COLORREF color, int penStyle);

/* Draw a polyline through bitmap points */
void Overlay_DrawPolyline(const OverlayContext* ctx, const POINT* ptsBitmap, int count,
                          COLORREF color, int penStyle);

/* Hit test box handles with zoom-adjusted tolerance (shared helper) */
int Overlay_HitTestBoxHandles(RECT* rcBmp, int xBmp, int yBmp);

#endif /* OVERLAY_H */
