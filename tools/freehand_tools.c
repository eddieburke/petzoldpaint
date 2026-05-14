/*------------------------------------------------------------------------------
 * FREEHAND_TOOLS.C
 *
 * Pencil, Brush, and Eraser only. Airbrush is intentionally separate because it
 * has timer-driven behavior instead of normal drag interpolation.
 *----------------------------------------------------------------------------*/

#include "freehand_tools.h"
#include "../canvas.h"
#include "../geom.h"
#include "../helpers.h"
#include "../layers.h"
#include "../palette.h"
#include "drawing_primitives.h"
#include "stroke_session.h"
#include "tool_options/tool_options.h"
#include <stdlib.h>

static StrokeSession s_session = {0};
typedef struct {
  BYTE *bits;
  int width;
  int height;
} ActiveSurface;

typedef void (*PointDrawFn)(BYTE *bits, int width, int height, int x, int y,
                            COLORREF color, int size);
typedef void (*LineDrawFn)(BYTE *bits, int width, int height, int x1, int y1,
                           int x2, int y2, COLORREF color, int size);

typedef struct {
  int toolId;
  PointDrawFn point;
  LineDrawFn line;
  int (*size)(void);
  BOOL useSecondaryColor;
} FreehandToolDef;

static int UnitSize(void) { return 1; }
static int BrushSize(void) { return nBrushWidth; }

static void PencilPoint(BYTE *bits, int width, int height, int x, int y,
                        COLORREF color, int size) {
  (void)size;
  DrawPrim_DrawPencilPoint(bits, width, height, x, y, color);
}

static void PencilLine(BYTE *bits, int width, int height, int x1, int y1,
                       int x2, int y2, COLORREF color, int size) {
  (void)size;
  DrawPrim_DrawPencilLine(bits, width, height, x1, y1, x2, y2, color);
}

static void BrushPoint(BYTE *bits, int width, int height, int x, int y,
                       COLORREF color, int size) {
  DrawPrim_DrawBrushPoint(bits, width, height, x, y, color, size);
}

static void BrushLine(BYTE *bits, int width, int height, int x1, int y1,
                      int x2, int y2, COLORREF color, int size) {
  DrawPrim_DrawBrushLine(bits, width, height, x1, y1, x2, y2, color, size);
}

static void EraserPoint(BYTE *bits, int width, int height, int x, int y,
                        COLORREF color, int size) {
  DrawPrim_DrawEraserPoint(bits, width, height, x, y, color, size);
}

static const FreehandToolDef *GetToolDef(int toolId) {
  static const FreehandToolDef pencil = {
      TOOL_PENCIL, PencilPoint, PencilLine, UnitSize, FALSE};
  static const FreehandToolDef brush = {
      TOOL_BRUSH, BrushPoint, BrushLine, BrushSize, FALSE};
  static const FreehandToolDef eraser = {
      TOOL_ERASER, EraserPoint, NULL, BrushSize, TRUE};

  switch (toolId) {
  case TOOL_PENCIL:
    return &pencil;
  case TOOL_BRUSH:
    return &brush;
  case TOOL_ERASER:
    return &eraser;
  default:
    return NULL;
  }
}

static COLORREF ToolColor(const FreehandToolDef *tool, int button) {
  return tool->useSecondaryColor ? Palette_GetSecondaryColor()
                                 : GetColorForButton(button);
}

static void MarkDirtyAroundLine(const FreehandToolDef *tool, int x1, int y1,
                                int x2, int y2) {
  int radius = DrawPrim_GetBrushSize(tool->size()) + 2;
  RECT rcDirty = {min(x1, x2) - radius, min(y1, y2) - radius,
                  max(x1, x2) + radius, max(y1, y2) + radius};
  RECT rcScreen;

  LayersMarkDirtyRect(&rcDirty);
  RectBmpToScr(&rcDirty, &rcScreen);
  InflateRect(&rcScreen, 2, 2);
  InvalidateCanvasRect(&rcScreen);
}

static ActiveSurface ActiveSurface_Get(void) {
  ActiveSurface s = {0};
  s.bits = LayersGetActiveColorBits();
  if (!s.bits) return s;
  s.width = Canvas_GetWidth();
  s.height = Canvas_GetHeight();
  return s;
}

static void InvalidateDirtyBitmapRect(const RECT *rcDirty) {
  LayersMarkDirtyRect(rcDirty);
  RECT rcScreen;
  RectBmpToScr(rcDirty, &rcScreen);
  InflateRect(&rcScreen, 2, 2);
  InvalidateCanvasRect(&rcScreen);
}

static void FreehandDrawInterpolated(BYTE *bits, int width, int height,
                                     const StrokePolicy *policy, int x1, int y1,
                                     int x2, int y2, COLORREF color) {
  const int size = policy->pfnGetSize();
  // Use the optimized line primitive if available for this configuration
  if (policy->pfnLine) {
    policy->pfnLine(bits, width, height, x1, y1, x2, y2, color, size);
  } else if (policy->bInterpolate) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = abs(dx) + abs(dy); // Manhattan distance for 4-connectivity

    if (steps == 0) {
      policy->pfnPoint(bits, width, height, x2, y2, color, size);
    } else {
      // High-density stepping loop fall-back
      for (int i = 1; i <= steps; i++) {
        int ix = x1 + (dx * i + (steps / 2 * (dx < 0 ? -1 : 1))) / steps;
        int iy = y1 + (dy * i + (steps / 2 * (dy < 0 ? -1 : 1))) / steps;
        policy->pfnPoint(bits, width, height, ix, iy, color, size);
      }
    }
  } else {
    policy->pfnPoint(bits, width, height, x2, y2, color, size);
  }
}

void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton,
                              int toolId) {
  const FreehandToolDef *tool = GetToolDef(toolId);
  if (!tool)
    return;

  if (s_session.isDrawing && nButton != s_session.drawButton) {
    CancelFreehandDrawing();
    return;
  }

  StrokeSession_Begin(&s_session, hWnd, x, y, nButton, toolId);

  ActiveSurface surface = ActiveSurface_Get();
  if (surface.bits) {
    sp->pfnPoint(surface.bits, surface.width, surface.height, x, y,
                     ResolveStrokeColor(sp, nButton), sp->pfnGetSize());
    LayersMarkDirty();
    StrokeSession_MarkPixelsModified(&s_session);

    int radius = DrawPrim_GetBrushSize(sp->pfnGetSize()) + 10;
    RECT rcDirty;
    rcDirty.left = x - radius;
    rcDirty.top = y - radius;
    rcDirty.right = x + radius;
    rcDirty.bottom = y + radius;
    InvalidateDirtyBitmapRect(&rcDirty);
  }
}

void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton,
                              int toolId) {
  (void)hWnd;
  (void)toolId;
  if (!s_session.isDrawing || !StrokeSession_IsActiveButton(nButton))
    return;

  const FreehandToolDef *tool = GetToolDef(s_session.toolId);
  BYTE *bits = LayersGetActiveColorBits();
  if (!tool || !bits)
    return;

  ActiveSurface surface = ActiveSurface_Get();
  if (surface.bits) {
    FreehandDrawInterpolated(surface.bits, surface.width, surface.height, sp, s_session.lastPoint.x,
                             s_session.lastPoint.y, x, y, ResolveStrokeColor(sp, s_session.drawButton));

    // Calculate dirty rect in BITMAP space
    int radius = DrawPrim_GetBrushSize(sp->pfnGetSize()) + 2;
    RECT rcDirty;
    rcDirty.left = min(s_session.lastPoint.x, x) - radius;
    rcDirty.top = min(s_session.lastPoint.y, y) - radius;
    rcDirty.right = max(s_session.lastPoint.x, x) + radius;
    rcDirty.bottom = max(s_session.lastPoint.y, y) + radius;

    // Send bitmap space to layer engine
    InvalidateDirtyBitmapRect(&rcDirty);
    StrokeSession_MarkPixelsModified(&s_session);
  }
  StrokeSession_UpdateLastPoint(&s_session, x, y);
}

void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton,
                            int toolId) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  (void)toolId;
  StrokeSession_CommitIfNeeded(&s_session, "Draw");
  StrokeSession_End(&s_session);
}

/*------------------------------------------------------------------------------
 * Airbrush Tool Public API
 *----------------------------------------------------------------------------*/


void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId) {
  BeginStroke(hWnd, x, y, nButton, GetStrokePolicy(toolId));
}

void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId) {
  (void)toolId;
  AppendPoint(hWnd, x, y, nButton);
}

void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId) {
  (void)toolId;
  EndStroke(hWnd, x, y, nButton);
}

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  BeginStroke(hWnd, x, y, nButton, GetStrokePolicy(TOOL_AIRBRUSH));
  if (s_session.isDrawing && hWnd) {
    SetTimer(hWnd, TIMER_AIRBRUSH, 30, NULL);
  }
}

void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd; AppendPoint(hWnd, x, y, nButton);
}

void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (hWnd) {
    KillTimer(hWnd, TIMER_AIRBRUSH);
  }
  EndStroke(hWnd, x, y, nButton);
}

void FreehandTool_OnTimerTick(void) {
  if (!s_session.isDrawing || s_session.toolId != TOOL_AIRBRUSH) return;

  ActiveSurface surface = ActiveSurface_Get();
  if (surface.bits) {
    int radius = DrawPrim_GetSprayRadius(nSprayRadius) + 2;
    RECT rcDirty = {s_session.lastPoint.x - radius, s_session.lastPoint.y - radius,
                    s_session.lastPoint.x + radius, s_session.lastPoint.y + radius};

    DrawPrim_DrawSprayPoint(surface.bits, surface.width, surface.height,
                            s_session.lastPoint.x, s_session.lastPoint.y,
                            GetColorForButton(s_session.drawButton), nSprayRadius);
    InvalidateDirtyBitmapRect(&rcDirty);
    StrokeSession_MarkPixelsModified(&s_session);
  }
}

/*------------------------------------------------------------------------------
  * Shared State Accessors
  *----------------------------------------------------------------------------*/

static void KillAirbrushTimerIfNeeded(HWND hWnd) {
  if (GetActiveFreehandTool() == TOOL_AIRBRUSH && hWnd) {
    KillTimer(hWnd, TIMER_AIRBRUSH);
  }
}

BOOL IsFreehandDrawing(void) { return s_session.isDrawing; }

void FreehandTool_Deactivate(void) {
  StrokeSession_End(&s_session);
}

BOOL CancelFreehandDrawing(void) {
  if (!s_session.isDrawing)
    return FALSE;
  StrokeSession_Cancel(&s_session);
  return TRUE;
}

int GetActiveFreehandTool(void) { return s_session.toolId; }
