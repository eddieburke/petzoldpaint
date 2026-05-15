#include "magnifier_tool.h"
#include "peztold_core.h"

#include "geom.h"
#include "helpers.h"
#include "gdi_utils.h"
#include "canvas.h"

static BOOL bMagnifierDragging = FALSE;
static int nMagnifierStartX = 0;
static int nMagnifierStartY = 0;
static int nMagnifierEndX = 0;
static int nMagnifierEndY = 0;

void MagnifierToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (nButton == MK_LBUTTON) {
    bMagnifierDragging = TRUE;
    nMagnifierStartX = x;
    nMagnifierStartY = y;
    nMagnifierEndX = x;
    nMagnifierEndY = y;
    SetCapture(hWnd);
  } else {
    Canvas_SetZoom(100.0);
    SendMessage(GetParent(hWnd), WM_SIZE, 0, 0);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
}

void MagnifierToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (bMagnifierDragging) {
    nMagnifierEndX = x;
    nMagnifierEndY = y;
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
}

void MagnifierToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (!bMagnifierDragging)
    return;

  ReleaseCapture();
  bMagnifierDragging = FALSE;

  int left = min(nMagnifierStartX, nMagnifierEndX);
  int top = min(nMagnifierStartY, nMagnifierEndY);
  int right = max(nMagnifierStartX, nMagnifierEndX);
  int bottom = max(nMagnifierStartY, nMagnifierEndY);
  int selWBitmap = right - left;
  int selHBitmap = bottom - top;

  if (selWBitmap < 4 || selHBitmap < 4) {
    if (Canvas_GetZoom() < 800) {
      if (Canvas_GetZoom() < 100)
        Canvas_SetZoom(100);
      else
        Canvas_SetZoom(Canvas_GetZoom() * 2);
    }
  } else {
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);
    int viewW = rcClient.right - rcClient.left;
    int viewH = rcClient.bottom - rcClient.top;

    double zoomW = (double)(viewW * 100.0) / selWBitmap;
    double zoomH = (double)(viewH * 100.0) / selHBitmap;
    Canvas_SetZoom(min(zoomW, zoomH));

    if (Canvas_GetZoom() < MIN_ZOOM_PERCENT)
      Canvas_SetZoom(MIN_ZOOM_PERCENT);
    if (Canvas_GetZoom() > MAX_ZOOM_PERCENT)
      Canvas_SetZoom(MAX_ZOOM_PERCENT);
  }

  SendMessage(GetParent(hWnd), WM_SIZE, 0, 0);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void MagnifierToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  if (!bMagnifierDragging)
    return;

  HPEN hOldPen;
  HPEN hPen = CreatePenAndSelect(hdc, PS_DOT, 1, RGB(0, 0, 0), &hOldPen);
  HBRUSH hBrushOld = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

  int x1, y1, x2, y2;
  Viewport vp = { dScale, nDestX, nDestY };
  Viewport_BmpToScr(&vp, nMagnifierStartX, nMagnifierStartY, &x1, &y1);
  Viewport_BmpToScr(&vp, nMagnifierEndX, nMagnifierEndY, &x2, &y2);

  Rectangle(hdc, x1, y1, x2, y2);

  SelectObject(hdc, hBrushOld);
  RestorePen(hdc, hOldPen);
  Gdi_DeletePen(hPen);
}

void MagnifierToolDeactivate(void) {
  if (bMagnifierDragging) {
    bMagnifierDragging = FALSE;
    ReleaseCapture();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
}
