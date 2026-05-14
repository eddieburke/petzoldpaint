/*------------------------------------------------------------------------------
 * POLYGON_TOOL.C
 *
 * Polygon Drawing Tool Implementation (Draft Layer model)
 *
 * MouseDown/Move: Draws open polygon + rubber band to the draft layer.
 * DoubleClick / close-to-start: Merges draft to active layer, pushes history.
 * onDeactivate / onCancel:      Clears draft, resets state.
 *----------------------------------------------------------------------------*/

#include "polygon_tool.h"
#include "../canvas.h"
#include "../peztold_core.h"

#include "../draw.h"
#include "../geom.h"
#include "../helpers.h"
#include "../history.h"
#include "../layers.h"
#include "../overlay.h"
#include "../poly_store.h"
#include "../resource.h"
#include "../tools.h"
#include "tool_options/tool_options.h"
#include <stdlib.h>
#include "../palette.h"

/*------------------------------------------------------------------------------
 * Internal State
 *----------------------------------------------------------------------------*/

static PolyStore polygon;
static BOOL bPolygonPending = FALSE;
static BOOL bDragging = FALSE;
static POINT ptRubberBand;
static int nDrawButton = 0;
static BOOL bSuspendingCapture = FALSE;
static int nDragIndex = -1;

/*------------------------------------------------------------------------------
 * FillPolygonAlpha
 *----------------------------------------------------------------------------*/

static void FillPolygonAlpha(BYTE *bits, int width, int height,
                              COLORREF color) {
  if (polygon.count < 3)
    return;

  int minY = polygon.points[0].y, maxY = minY;
  for (int i = 1; i < polygon.count; i++) {
    if (polygon.points[i].y < minY)
      minY = polygon.points[i].y;
    if (polygon.points[i].y > maxY)
      maxY = polygon.points[i].y;
  }
  if (minY < 0)
    minY = 0;
  if (maxY >= height)
    maxY = height - 1;

  int maxIntersections = polygon.count * 2 + 16;
  int *intersections = (int *)malloc(maxIntersections * sizeof(int));
  if (!intersections)
    return;

  for (int y = minY; y <= maxY; y++) {
    int numIntersections = 0;
    for (int i = 0;
         i < polygon.count && numIntersections < maxIntersections - 2; i++) {
      int j = (i + 1) % polygon.count;
      int y1 = polygon.points[i].y;
      int y2 = polygon.points[j].y;
      if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
        int x1 = polygon.points[i].x;
        int x2 = polygon.points[j].x;
        intersections[numIntersections++] =
            x1 + (y - y1) * (x2 - x1) / (y2 - y1);
      }
    }
    for (int i = 0; i < numIntersections - 1; i++)
      for (int j = i + 1; j < numIntersections; j++)
        if (intersections[i] > intersections[j]) {
          int tmp = intersections[i];
          intersections[i] = intersections[j];
          intersections[j] = tmp;
        }
    for (int i = 0; i + 1 < numIntersections; i += 2) {
      int x1 = intersections[i], x2 = intersections[i + 1];
      if (x1 < 0) x1 = 0;
      if (x2 >= width) x2 = width - 1;
      for (int x = x1; x <= x2; x++)
        DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    }
  }
  free(intersections);
}

/*------------------------------------------------------------------------------
 * DrawPolygonToDraft
 *----------------------------------------------------------------------------*/

static void DrawPolygonToDraft(void) {
  if (!bPolygonPending || polygon.count == 0)
    return;

  LayersClearDraft();
  BYTE *bits = LayersGetDraftBits();
  if (!bits)
    return;

  COLORREF fg = (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
  float radius = (nBrushWidth > 0 ? nBrushWidth : 1) / 2.0f;

  for (int i = 0; i < polygon.count - 1; i++) {
    DrawLineAAAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), (float)polygon.points[i].x,
                    (float)polygon.points[i].y, (float)polygon.points[i + 1].x,
                    (float)polygon.points[i + 1].y, radius, fg, 255, LAYER_BLEND_NORMAL);
  }

  if (polygon.count > 0) {
    POINT last = polygon.points[polygon.count - 1];
    DrawLineAAAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), (float)last.x,
                    (float)last.y, (float)ptRubberBand.x, (float)ptRubberBand.y,
                    radius, fg, 128, LAYER_BLEND_NORMAL);
  }

  LayersMarkDirty();
}

/*------------------------------------------------------------------------------
 * CommitPolygon
 *----------------------------------------------------------------------------*/

static void CommitPolygonInternal(void) {
  if (!bPolygonPending || polygon.count < 3) {
    LayersClearDraft();
    bPolygonPending = FALSE;
    Poly_Free(&polygon);
    InvalidateCanvas();
    return;
  }

  LayersClearDraft();
  BYTE *bits = LayersGetDraftBits();
  if (bits) {
    COLORREF fg = (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
    COLORREF bg = (nDrawButton == MK_RBUTTON) ? Palette_GetPrimaryColor() : Palette_GetSecondaryColor();
    float radius = (nBrushWidth > 0 ? nBrushWidth : 1) / 2.0f;

    if ((nShapeDrawType == 1 || nShapeDrawType == 2) && polygon.count >= 3) {
      FillPolygonAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                       (nShapeDrawType == 1) ? fg : bg);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2) {
      for (int i = 0; i < polygon.count; i++) {
        int next = (i + 1) % polygon.count;
        DrawLineAAAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                        (float)polygon.points[i].x, (float)polygon.points[i].y,
                        (float)polygon.points[next].x,
                        (float)polygon.points[next].y, radius, fg, 255, LAYER_BLEND_NORMAL);
      }
    }
  }

  LayersMergeDraftToActive();
  LayersMarkDirty();
  SetDocumentDirty();
  HistoryPushToolActionById(TOOL_POLYGON, "Draw Polygon");

  bPolygonPending = FALSE;
  Poly_Free(&polygon);
  InvalidateCanvas();
}

void CommitPendingPolygon(void) { CommitPolygonInternal(); }
BOOL IsPolygonPending(void) { return bPolygonPending; }

/*------------------------------------------------------------------------------
 * Lifecycle hooks
 *----------------------------------------------------------------------------*/

BOOL PolygonTool_Cancel(void) {
  if (bSuspendingCapture)
    return FALSE;

  LayersClearDraft();
  Poly_Free(&polygon);
  bPolygonPending = FALSE;
  InvalidateCanvas();
  return TRUE;
}

void PolygonTool_Deactivate(void) {
  if (bPolygonPending) {
    LayersClearDraft();
    Poly_Free(&polygon);
    bPolygonPending = FALSE;
    InvalidateCanvas();
  }
}

static BOOL PolygonTryAddPoint(HWND hWnd, int x, int y);

static int HitTestVertex(int x, int y) {
   if (polygon.count == 0) return -1;
   double scale = GetZoomScale();
   int tol = (int)(8.0 / scale);
   if (tol < 2) tol = 2;
   for (int i = 0; i < polygon.count; i++) {
     int dx = x - polygon.points[i].x;
     int dy = y - polygon.points[i].y;
     if (dx * dx + dy * dy <= tol * tol)
       return i;
   }
   return -1;
}

static BOOL PolygonTryAddPoint(HWND hWnd, int x, int y) {
  if (!Poly_Add(&polygon, x, y)) {
    PolygonTool_Cancel();
    if (GetCapture() == hWnd)
      ReleaseCapture();
    return FALSE;
  }
  return TRUE;
}

/*------------------------------------------------------------------------------
 * Event Handlers
 *----------------------------------------------------------------------------*/

void PolygonTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (nButton == MK_RBUTTON) {
    if (bPolygonPending && polygon.count >= 3)
      CommitPolygonInternal();
    return;
  }

if (!bPolygonPending) {
     nDrawButton = nButton;
     Poly_Init(&polygon);
     if (!PolygonTryAddPoint(hWnd, x, y))
       return;
     bPolygonPending = TRUE;
   } else {
     nDragIndex = HitTestVertex(x, y);
      if (nDragIndex >= 0) {
        bDragging = TRUE;
        ptRubberBand.x = x;
        ptRubberBand.y = y;
        SetCapture(hWnd);
        InvalidateCanvas();
        return;
      }
     if (polygon.count >= 3) {
      int xScr, yScr, xStartScr, yStartScr;
      CoordBmpToScr(x, y, &xScr, &yScr);
      CoordBmpToScr(polygon.points[0].x, polygon.points[0].y, &xStartScr, &yStartScr);

      int distSq = DistSq(xScr, yScr, xStartScr, yStartScr);
      if (distSq < 100) {
        CommitPolygonInternal();
        return;
      }
    }
    if (!PolygonTryAddPoint(hWnd, x, y))
      return;
  }

  ptRubberBand.x = x;
  ptRubberBand.y = y;
  bDragging = TRUE;
  SetCapture(hWnd);
  DrawPolygonToDraft();
  InvalidateCanvas();
}

void PolygonTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (!bPolygonPending)
    return;

  if (nDragIndex >= 0 && polygon.points) {
    polygon.points[nDragIndex].x = x;
    polygon.points[nDragIndex].y = y;
    DrawPolygonToDraft();
    InvalidateCanvas();
    return;
  }

  int endX = x, endY = y;

  if (bDragging) {
    if (polygon.count > 0) {
      if (IsShiftDown() && polygon.count > 1) {
        POINT prev = polygon.points[polygon.count - 2];
        SnapToAngle(prev.x, prev.y, &endX, &endY, SNAP_ANGLE_DEG);
      }
      Poly_SetLast(&polygon, endX, endY);
    }
  } else {
    if (IsShiftDown() && polygon.count > 0) {
      POINT last = Poly_GetLast(&polygon);
      SnapToAngle(last.x, last.y, &endX, &endY, SNAP_ANGLE_DEG);
    }
  }

  ptRubberBand.x = endX;
  ptRubberBand.y = endY;
  DrawPolygonToDraft();
  InvalidateCanvas();
}

void PolygonTool_OnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (GetCapture() == hWnd) {
    bSuspendingCapture = TRUE;
    ReleaseCapture();
    bSuspendingCapture = FALSE;
  }
  bDragging = FALSE;
  nDragIndex = -1;
  ptRubberBand.x = x;
  ptRubberBand.y = y;
  if (bPolygonPending) {
    DrawPolygonToDraft();
    InvalidateCanvas();
  }
}

void PolygonTool_OnDoubleClick(HWND hWnd, int x, int y, int nButton) {
  if (bPolygonPending) {
    if (!PolygonTryAddPoint(hWnd, x, y))
      return;
    if (polygon.count >= 3)
      CommitPolygonInternal();
  }
}

void PolygonTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  if (!bPolygonPending || polygon.count == 0)
    return;

  OverlayContext ctx;
  Overlay_Init(&ctx, hdc, dScale, nDestX, nDestY);

  COLORREF fg = (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();

  if (nDragIndex < 0 && polygon.count > 0) {
    POINT last = Poly_GetLast(&polygon);
    Overlay_DrawLine(&ctx, last.x, last.y, ptRubberBand.x, ptRubberBand.y, fg,
                     PS_SOLID);
    if (polygon.count >= 2) {
      Overlay_DrawLine(&ctx, ptRubberBand.x, ptRubberBand.y,
                       polygon.points[0].x, polygon.points[0].y,
                       RGB(128, 128, 128), PS_DOT);
    }
  }

  Overlay_DrawPolyHandles(&ctx, polygon.points, polygon.count);
}
