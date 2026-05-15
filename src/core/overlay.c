#include "overlay.h"
#include "canvas.h"
#include "draw.h"
#include "gdi_utils.h"
#include "geom.h"
#include "helpers.h"
#include <stdlib.h>
#define HANDLE_GRAB 5

void Overlay_Init(OverlayContext *ctx, HDC hdc, double dScale, int nDestX,
                  int nDestY) {
  ctx->hdc = hdc;
  ctx->vp.scale = dScale;
  ctx->vp.offX = nDestX;
  ctx->vp.offY = nDestY;
}

void Overlay_DrawHandle(const OverlayContext *ctx, int xBmp, int yBmp, int type,
                        BOOL bHollow) {
  int xScr, yScr;
  CoordBmpToScr(xBmp, yBmp, &xScr, &yScr);

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

int Overlay_HitTestBoxHandles(const RECT *rcBmp, int xBmp, int yBmp) {
  double dScale = GetZoomScale();
  int tolerance = (int)(6.0 / dScale);
  if (tolerance < 1)
    tolerance = 1;
  return HitTestBoxHandles(rcBmp, xBmp, yBmp, tolerance);
}


void Overlay_DrawSelectionFrame(const OverlayContext *ctx, const RECT *rcBitmap,
                                BOOL bDotted) {
  // Convert bitmap rectangle to screen coordinates
  RECT rcScreen;
  Viewport_RectBmpToScr(&ctx->vp, rcBitmap, &rcScreen);

  // Use helper function from helpers.c
  DrawSelectionFrame(ctx->hdc, &rcScreen, bDotted);
}


void Overlay_DrawLine(const OverlayContext *ctx, int x1Bmp, int y1Bmp,
                      int x2Bmp, int y2Bmp, COLORREF color, int penStyle) {
  int x1Scr, y1Scr, x2Scr, y2Scr;
  CoordBmpToScr(x1Bmp, y1Bmp, &x1Scr, &y1Scr);
  CoordBmpToScr(x2Bmp, y2Bmp, &x2Scr, &y2Scr);

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
    CoordBmpToScr(ptsBitmap[i].x, ptsBitmap[i].y, &ptsScreen[i].x,
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
