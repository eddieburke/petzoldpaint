/*------------------------------------------------------------------------------
 * FREEHAND_TOOLS.C
 *
 * Freehand Drawing Tools Implementation
 *
 * Implements the core logic for Pencil, Brush, Eraser, and Airbrush tools.
 * Handles mouse events, capture, and interpolation for smooth drawing.
 *----------------------------------------------------------------------------*/

#include "freehand_tools.h"
#include "../canvas.h"
#include "../geom.h"       /* <-- Added for RectBmpToScr */
#include "../helpers.h"
#include "../controller.h"
#include "../palette.h"
#include "../layers.h"
#include "../ui/widgets/colorbox.h"
#include "drawing_primitives.h"
#include "tool_options/tool_options.h"
#include "stroke_session.h"
#include <stdlib.h>

/*------------------------------------------------------------------------------
 * Localized Drawing State
 *----------------------------------------------------------------------------*/

static StrokeSession s_session = {0};
typedef struct {
  BYTE *bits;
  int width;
  int height;
} ActiveSurface;

typedef void (*PointDrawSizedFn)(BYTE *bits, int width, int height, int x, int y,
                                 COLORREF color, int size);
typedef void (*LineDrawSizedFn)(BYTE *bits, int width, int height, int x1, int y1,
                                int x2, int y2, COLORREF color, int size);

typedef enum {
  STROKE_COLOR_FROM_BUTTON = 0,
  STROKE_COLOR_BACKGROUND
} StrokeColorSource;

typedef enum {
  STROKE_COMPOSITE_NORMAL = 0,
  STROKE_COMPOSITE_ERASE
} StrokeCompositeMode;

struct FreehandStrokePolicy {
  PointDrawSizedFn pfnPoint;
  LineDrawSizedFn pfnLine;
  int (*pfnGetSize)(void);
  StrokeColorSource colorSource;
  StrokeCompositeMode composite;
  BOOL bInterpolate;
  BOOL bSmooth;
  BOOL bSpacing;
  int toolId;
};
typedef struct FreehandStrokePolicy StrokePolicy;

static void DrawPencilPointSized(BYTE *bits, int width, int height, int x, int y,
                                 COLORREF color, int size) {
  (void)size;
  DrawPrim_DrawPencilPoint(bits, width, height, x, y, color);
}

static void DrawPencilLineSized(BYTE *bits, int width, int height, int x1, int y1,
                                int x2, int y2, COLORREF color, int size) {
  (void)size;
  DrawPrim_DrawPencilLine(bits, width, height, x1, y1, x2, y2, color);
}

static void DrawBrushPointSized(BYTE *bits, int width, int height, int x, int y,
                                COLORREF color, int size) {
  DrawPrim_DrawBrushPoint(bits, width, height, x, y, color, size);
}

static void DrawBrushLineSized(BYTE *bits, int width, int height, int x1, int y1,
                               int x2, int y2, COLORREF color, int size) {
  DrawPrim_DrawBrushLine(bits, width, height, x1, y1, x2, y2, color, size);
}

static void DrawEraserPointSized(BYTE *bits, int width, int height, int x, int y,
                                 COLORREF color, int size) {
  DrawPrim_DrawEraserPoint(bits, width, height, x, y, color, size);
}

static void DrawSprayPointSized(BYTE *bits, int width, int height, int x, int y,
                                COLORREF color, int size) {
  DrawPrim_DrawSprayPoint(bits, width, height, x, y, color, size);
}

static int GetUnitSize(void) { return 1; }
static int GetBrushWidthSize(void) { return nBrushWidth; }
static int GetSprayRadiusSize(void) { return nSprayRadius; }

static const StrokePolicy *GetStrokePolicy(int tool) {
  static const StrokePolicy policies[] = {
      [TOOL_PENCIL] = {DrawPencilPointSized, DrawPencilLineSized, GetUnitSize,
                       STROKE_COLOR_FROM_BUTTON, STROKE_COMPOSITE_NORMAL, TRUE, FALSE, FALSE, TOOL_PENCIL},
      [TOOL_BRUSH] = {DrawBrushPointSized, DrawBrushLineSized, GetBrushWidthSize,
                      STROKE_COLOR_FROM_BUTTON, STROKE_COMPOSITE_NORMAL, TRUE, TRUE, FALSE, TOOL_BRUSH},
      [TOOL_ERASER] = {DrawEraserPointSized, NULL, GetBrushWidthSize,
                       STROKE_COLOR_BACKGROUND, STROKE_COMPOSITE_ERASE, TRUE, FALSE, FALSE, TOOL_ERASER},
      [TOOL_AIRBRUSH] = {DrawSprayPointSized, NULL, GetSprayRadiusSize,
                         STROKE_COLOR_FROM_BUTTON, STROKE_COMPOSITE_NORMAL, FALSE, FALSE, TRUE, TOOL_AIRBRUSH},
  };
  typedef char StrokePolicyCoverage[(TOOL_AIRBRUSH < (int)(sizeof(policies) / sizeof(policies[0]))) ? 1 : -1];
  (void)sizeof(StrokePolicyCoverage);

  if (tool < 0 || tool >= (int)(sizeof(policies) / sizeof(policies[0]))) return NULL;
  if (!policies[tool].pfnGetSize) return NULL;
  return &policies[tool];
}

static COLORREF ResolveStrokeColor(const StrokePolicy *policy, int button) {
  if (policy->composite == STROKE_COMPOSITE_ERASE ||
      policy->colorSource == STROKE_COLOR_BACKGROUND) {
    return Palette_GetSecondaryColor();
  }
  return GetColorForButton(button);
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

/*------------------------------------------------------------------------------
 * Unified Event Handlers
 *----------------------------------------------------------------------------*/

static void BeginStroke(HWND hWnd, int x, int y, int nButton, const StrokePolicy *sp) {
  int tool = sp ? sp->toolId : s_session.toolId;
  if (s_session.isDrawing && nButton != s_session.drawButton) {
    CancelFreehandDrawing();
    return;
  }

  if (!sp) sp = GetStrokePolicy(tool);
  if (!sp || !sp->pfnPoint)
    return;

  StrokeSession_Begin(&s_session, hWnd, x, y, nButton, tool);

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

static void AppendPoint(HWND hWnd, int x, int y, int nButton) {
  if (!s_session.isDrawing || !StrokeSession_IsActiveButton(nButton))
    return;

  const StrokePolicy *sp = GetStrokePolicy(s_session.toolId);
  if (!sp || !sp->pfnPoint)
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
  InvalidateCanvas();
}

static void EndStroke(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
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
   if (s_session.isDrawing) {
     KillAirbrushTimerIfNeeded(GetCanvasWindow());
     StrokeSession_End(&s_session);
   }
}


BOOL CancelFreehandDrawing(void) {
   BOOL bWasDrawing = s_session.isDrawing;
   if (s_session.isDrawing) {
     KillAirbrushTimerIfNeeded(GetCanvasWindow());
     StrokeSession_Cancel(&s_session);
   }
   return bWasDrawing;
}

int GetActiveFreehandTool(void) { return s_session.toolId; }
