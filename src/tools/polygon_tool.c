#include "polygon_tool.h"
#include "canvas.h"
#include "peztold_core.h"

#include "draw.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "interaction.h"
#include "layers.h"
#include "overlay.h"
#include "commit_bar.h"
#include "poly_store.h"
#include "tools.h"
#include "tool_options/tool_options.h"
#include <stdlib.h>
#include "palette.h"

static PolyStore polygon;
static BOOL bPolygonPending = FALSE;
static BOOL bDragging = FALSE;
static POINT ptRubberBand;
static int nDrawButton = 0;
static BOOL bSuspendingCapture = FALSE;
static int nDragIndex = -1;
static BOOL bVertexMoved = FALSE;

static void FillPolygonAlpha(BYTE *bits, int width, int height,
                              COLORREF color, BYTE alpha) {
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
        DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    }
  }
  free(intersections);
}

static void DrawPolygonToDraft(void) {
  if (!bPolygonPending || polygon.count == 0)
    return;

  LayersClearDraft();
  BYTE *bits = LayersGetDraftBits();
  if (!bits)
    return;

  COLORREF fg =
      (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
  BYTE fgAlpha =
      (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryOpacity() : Palette_GetPrimaryOpacity();
  int lineW = (nBrushWidth > 0 ? nBrushWidth : 1);

  for (int i = 0; i < polygon.count - 1; i++) {
    DrawLineAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                  polygon.points[i].x, polygon.points[i].y,
                  polygon.points[i + 1].x, polygon.points[i + 1].y, lineW, fg,
                  fgAlpha, LAYER_BLEND_NORMAL);
  }

  if (polygon.count > 0) {
    POINT last = polygon.points[polygon.count - 1];
    DrawLineAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), last.x, last.y,
                  ptRubberBand.x, ptRubberBand.y, lineW, fg,
                  (BYTE)(fgAlpha / 2), LAYER_BLEND_NORMAL);
  }

  LayersMarkDirty();
}

static void CommitPolygonInternal(void) {
  if (!bPolygonPending || polygon.count < 3) {
    if (Interaction_IsActive())
      Interaction_Abort();
    LayersClearDraft();
    bPolygonPending = FALSE;
    Poly_Free(&polygon);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return;
  }

  LayersClearDraft();
  BYTE *bits = LayersGetDraftBits();
  if (bits) {
    COLORREF fg =
        (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
    COLORREF bg =
        (nDrawButton == MK_RBUTTON) ? Palette_GetPrimaryColor() : Palette_GetSecondaryColor();
    BYTE fgAlpha =
        (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryOpacity() : Palette_GetPrimaryOpacity();
    BYTE bgAlpha =
        (nDrawButton == MK_RBUTTON) ? Palette_GetPrimaryOpacity() : Palette_GetSecondaryOpacity();
    int lineW = (nBrushWidth > 0 ? nBrushWidth : 1);

    if ((nShapeDrawType == 1 || nShapeDrawType == 2) && polygon.count >= 3) {
      FillPolygonAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                       (nShapeDrawType == 1) ? fg : bg,
                       (nShapeDrawType == 1) ? fgAlpha : bgAlpha);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2) {
      for (int i = 0; i < polygon.count; i++) {
        int next = (i + 1) % polygon.count;
        DrawLineAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                      polygon.points[i].x, polygon.points[i].y,
                      polygon.points[next].x, polygon.points[next].y, lineW, fg,
                      fgAlpha, LAYER_BLEND_NORMAL);
      }
    }
  }

  Interaction_Commit("Draw Polygon");

  bPolygonPending = FALSE;
  Poly_Free(&polygon);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
BOOL IsPolygonPending(void) { return bPolygonPending; }
void PolygonTool_CommitPending(void) { CommitPolygonInternal(); }

BOOL PolygonTool_Cancel(void) {
  if (bSuspendingCapture)
    return FALSE;

  if (Interaction_IsActive())
    Interaction_Abort();
  LayersClearDraft();
  Poly_Free(&polygon);
  bPolygonPending = FALSE;
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  return TRUE;
}

void PolygonTool_Deactivate(void) {
  if (bPolygonPending) {
    if (Interaction_IsActive())
      Interaction_Abort();
    LayersClearDraft();
    Poly_Free(&polygon);
    bPolygonPending = FALSE;
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
}

static void StartNewPolygon(HWND hWnd, int x, int y, int nButton);
static void HandleVertexDrag(HWND hWnd, int x, int y, int index);
static BOOL TryCommitOnClose(int x, int y);
static void AddPoint(HWND hWnd, int x, int y);

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

static void StartNewPolygon(HWND hWnd, int x, int y, int nButton) {
    nDrawButton = nButton;
    Poly_Init(&polygon);
    if (!PolygonTryAddPoint(hWnd, x, y))
        return;
    bPolygonPending = TRUE;
    Interaction_Begin(hWnd, x, y, nButton, TOOL_POLYGON);
    HistoryPushSession("Create Polygon");

    ptRubberBand.x = x;
    ptRubberBand.y = y;
    bDragging = TRUE;
    SetCapture(hWnd);
    DrawPolygonToDraft();
}

static void HandleVertexDrag(HWND hWnd, int x, int y, int index) {
    nDragIndex = index;
    bDragging = TRUE;
    bVertexMoved = FALSE;
    ptRubberBand.x = x;
    ptRubberBand.y = y;
    SetCapture(hWnd);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static BOOL TryCommitOnClose(int x, int y) {
    if (polygon.count < 3) return FALSE;
    int xScr, yScr, xStartScr, yStartScr;
    CoordBmpToScr(x, y, &xScr, &yScr);
    CoordBmpToScr(polygon.points[0].x, polygon.points[0].y, &xStartScr, &yStartScr);

    if (DistSq(xScr, yScr, xStartScr, yStartScr) < 100) {
        CommitPolygonInternal();
        return TRUE;
    }
    return FALSE;
}

static void AddPoint(HWND hWnd, int x, int y) {
    if (!PolygonTryAddPoint(hWnd, x, y)) return;
    HistoryPushSession("Adjust Polygon");

    ptRubberBand.x = x;
    ptRubberBand.y = y;
    bDragging = TRUE;
    SetCapture(hWnd);
    DrawPolygonToDraft();
}

void PolygonTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
    if (nButton == MK_RBUTTON) {
        if (bPolygonPending) {
            if (polygon.count >= 3)
                CommitPolygonInternal();
            else
                PolygonTool_Cancel();
        }
        return;
    }

    if (!bPolygonPending) {
        StartNewPolygon(hWnd, x, y, nButton);
    } else {
        RECT rcBounds = GetBoundingBox(polygon.points, polygon.count);
        COMMIT_BAR_HANDLE_CLICK(&rcBounds, x, y, CommitPolygonInternal(),
                                PolygonTool_Cancel());

        int dragIdx = HitTestVertex(x, y);
        if (dragIdx >= 0) {
            HandleVertexDrag(hWnd, x, y, dragIdx);
            return;
        }

        if (TryCommitOnClose(x, y))
            return;

        AddPoint(hWnd, x, y);
    }

    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void PolygonTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (!bPolygonPending)
    return;

  if (nDragIndex >= 0 && polygon.points) {
    polygon.points[nDragIndex].x = x;
    polygon.points[nDragIndex].y = y;
    bVertexMoved = TRUE;
    DrawPolygonToDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
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
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void PolygonTool_OnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (GetCapture() == hWnd) {
    bSuspendingCapture = TRUE;
    ReleaseCapture();
    bSuspendingCapture = FALSE;
  }
  BOOL wasDraggingVertex = (nDragIndex >= 0);
  bDragging = FALSE;
  nDragIndex = -1;
  ptRubberBand.x = x;
  ptRubberBand.y = y;
  if (bPolygonPending) {
    DrawPolygonToDraft();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
  if (wasDraggingVertex && bVertexMoved) {
    HistoryPushSession("Adjust Polygon");
  }
  bVertexMoved = FALSE;
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

  COLORREF fg =
      (nDrawButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();

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
  if (bPolygonPending && polygon.count > 0) {
    RECT rcBounds = GetBoundingBox(polygon.points, polygon.count);
    CommitBar_Draw(&ctx, &rcBounds);
  }
}

PolygonToolSnapshot *PolygonTool_CreateSnapshot(void) {
  if (!bPolygonPending)
    return NULL;

  PolygonToolSnapshot *snapshot =
      (PolygonToolSnapshot *)calloc(1, sizeof(PolygonToolSnapshot));
  if (!snapshot)
    return NULL;

  snapshot->bPolygonPending = bPolygonPending;
  snapshot->bDragging = bDragging;
  snapshot->ptRubberBand = ptRubberBand;
  snapshot->nDrawButton = nDrawButton;
  snapshot->nDragIndex = nDragIndex;
  if (!Poly_Copy(&snapshot->polygon, &polygon)) {
    free(snapshot);
    return NULL;
  }
  return snapshot;
}

void PolygonTool_DestroySnapshot(PolygonToolSnapshot *snapshot) {
  if (!snapshot)
    return;
  Poly_Free(&snapshot->polygon);
  free(snapshot);
}

void PolygonTool_ApplySnapshot(const PolygonToolSnapshot *snapshot) {
  if (Interaction_IsActive())
    Interaction_EndQuiet();
  LayersClearDraft();
  Poly_Free(&polygon);
  bPolygonPending = FALSE;
  bDragging = FALSE;
  nDragIndex = -1;
  bVertexMoved = FALSE;

  if (!snapshot) {
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return;
  }

  bPolygonPending = snapshot->bPolygonPending;
  bDragging = FALSE;
  ptRubberBand = snapshot->ptRubberBand;
  nDrawButton = snapshot->nDrawButton;
  nDragIndex = -1;
  Poly_Init(&polygon);
  Poly_Copy(&polygon, &snapshot->polygon);

  if (bPolygonPending && polygon.count > 0 && !Interaction_IsActive()) {
    POINT p0 = polygon.points[0];
    Interaction_BeginEx(GetCanvasWindow(), p0.x, p0.y, nDrawButton, TOOL_POLYGON,
                        FALSE);
  }

  DrawPolygonToDraft();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
