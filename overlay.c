/*------------------------------------------------------------
   OVERLAY.C -- Screen-Space Overlay Drawing

   This module provides functions for drawing UI overlays in
   screen-space coordinates, ensuring they remain crisp at
   any zoom level.
  ------------------------------------------------------------*/

#include "overlay.h"
#include "canvas.h"
#include "draw.h"
#include "gdi_utils.h"
#include "geom.h"
#include "helpers.h"
#include <stdlib.h>

/*------------------------------------------------------------
   Constants
  ------------------------------------------------------------*/

#define HANDLE_SIZE 5
#define HANDLE_GRAB 5

/*------------------------------------------------------------
   Context Initialization
  ------------------------------------------------------------*/

void Overlay_Init(OverlayContext *ctx, HDC hdc, double dScale, int nDestX,
                  int nDestY) {
  ctx->hdc = hdc;
  ctx->dScale = dScale;
  ctx->nDestX = nDestX;
  ctx->nDestY = nDestY;
}

void Overlay_BmpToScr(const OverlayContext *ctx, int xBmp, int yBmp, int *xScr,
                      int *yScr) {
  CoordBmpToScrEx(xBmp, yBmp, xScr, yScr, ctx->dScale, ctx->nDestX,
                  ctx->nDestY);
}

/*------------------------------------------------------------
   Handle Drawing Functions
  ------------------------------------------------------------*/

void Overlay_DrawHandle(const OverlayContext *ctx, int xBmp, int yBmp, int type,
                        BOOL bHollow) {
  int xScr, yScr;
  Overlay_BmpToScr(ctx, xBmp, yBmp, &xScr, &yScr);

  HBRUSH hBr = bHollow ? (HBRUSH)GetStockObject(NULL_BRUSH)
                       : (HBRUSH)GetStockObject(WHITE_BRUSH);
  HBRUSH hOld = SelectObject(ctx->hdc, hBr);
  HPEN hPen = GetStockObject(BLACK_PEN);
  HPEN hOldPen = SelectObject(ctx->hdc, hPen);
  int r = HANDLE_SIZE / 2;

  if (type == OVERLAY_HANDLE_SQUARE) {
    Rectangle(ctx->hdc, xScr - r, yScr - r, xScr + r + 1, yScr + r + 1);
  } else {
    Ellipse(ctx->hdc, xScr - r, yScr - r, xScr + r + 1, yScr + r + 1);
  }

  SelectObject(ctx->hdc, hOld);
  SelectObject(ctx->hdc, hOldPen);
}

void Overlay_DrawBoxHandles(const OverlayContext *ctx, const RECT *rcBitmap) {
  int xm = (rcBitmap->left + rcBitmap->right) / 2;
  int ym = (rcBitmap->top + rcBitmap->bottom) / 2;

  Overlay_DrawHandle(ctx, rcBitmap->left, rcBitmap->top, OVERLAY_HANDLE_SQUARE,
                     FALSE);
  Overlay_DrawHandle(ctx, xm, rcBitmap->top, OVERLAY_HANDLE_SQUARE, FALSE);
  Overlay_DrawHandle(ctx, rcBitmap->right, rcBitmap->top, OVERLAY_HANDLE_SQUARE,
                     FALSE);
  Overlay_DrawHandle(ctx, rcBitmap->right, ym, OVERLAY_HANDLE_SQUARE, FALSE);
  Overlay_DrawHandle(ctx, rcBitmap->right, rcBitmap->bottom,
                     OVERLAY_HANDLE_SQUARE, FALSE);
  Overlay_DrawHandle(ctx, xm, rcBitmap->bottom, OVERLAY_HANDLE_SQUARE, FALSE);
  Overlay_DrawHandle(ctx, rcBitmap->left, rcBitmap->bottom,
                     OVERLAY_HANDLE_SQUARE, FALSE);
  Overlay_DrawHandle(ctx, rcBitmap->left, ym, OVERLAY_HANDLE_SQUARE, FALSE);
}

void Overlay_DrawPolyHandles(const OverlayContext *ctx, const POINT *ptsBitmap,
                             int count) {
  for (int i = 0; i < count; i++) {
    Overlay_DrawHandle(ctx, ptsBitmap[i].x, ptsBitmap[i].y,
                       OVERLAY_HANDLE_CIRCLE, FALSE);
  }
}

/*------------------------------------------------------------
   Hit Testing with Zoom Adjustment
  ------------------------------------------------------------*/

int Overlay_HitTestBoxHandles(RECT *rcBmp, int xBmp, int yBmp) {
  double dScale = GetZoomScale();
  int tolerance = (int)(6.0 / dScale);
  if (tolerance < 1)
    tolerance = 1;
  return HitTestBoxHandlesEx(rcBmp, xBmp, yBmp, tolerance);
}

/*------------------------------------------------------------
   Selection Frame Drawing Functions
  ------------------------------------------------------------*/

void Overlay_DrawSelectionFrame(const OverlayContext *ctx, const RECT *rcBitmap,
                                BOOL bDotted) {
  // Convert bitmap rectangle to screen coordinates
  RECT rcScreen;
  RectBmpToScrEx(rcBitmap, &rcScreen, ctx->dScale, ctx->nDestX, ctx->nDestY);

  // Use helper function from helpers.c
  DrawSelectionFrame(ctx->hdc, &rcScreen, bDotted);
}

/*------------------------------------------------------------
   Line Drawing Functions
------------------------------------------------------------*/

void Overlay_DrawLine(const OverlayContext *ctx, int x1Bmp, int y1Bmp,
                      int x2Bmp, int y2Bmp, COLORREF color, int penStyle) {
  int x1Scr, y1Scr, x2Scr, y2Scr;
  Overlay_BmpToScr(ctx, x1Bmp, y1Bmp, &x1Scr, &y1Scr);
  Overlay_BmpToScr(ctx, x2Bmp, y2Bmp, &x2Scr, &y2Scr);

  HPEN hOldPen = NULL;
  HPEN hPen = CreatePenAndSelect(ctx->hdc, penStyle, 1, color, &hOldPen);

  MoveToEx(ctx->hdc, x1Scr, y1Scr, NULL);
  LineTo(ctx->hdc, x2Scr, y2Scr);

  RestorePen(ctx->hdc, hOldPen);
  Gdi_DeletePen(hPen);
}

void Overlay_DrawPolyline(const OverlayContext *ctx, const POINT *ptsBitmap,
                          int count, COLORREF color, int penStyle) {
  if (count < 2)
    return;

  POINT *ptsScreen = (POINT *)malloc(count * sizeof(POINT));
  if (!ptsScreen)
    return;

  for (int i = 0; i < count; i++) {
    Overlay_BmpToScr(ctx, ptsBitmap[i].x, ptsBitmap[i].y, &ptsScreen[i].x,
                     &ptsScreen[i].y);
  }

  HPEN hOldPen = NULL;
  HPEN hPen = CreatePenAndSelect(ctx->hdc, penStyle, 1, color, &hOldPen);
  if (hPen) {
    Polyline(ctx->hdc, ptsScreen, count);
    RestorePen(ctx->hdc, hOldPen);
    Gdi_DeletePen(hPen);
  }

  free(ptsScreen);
}
