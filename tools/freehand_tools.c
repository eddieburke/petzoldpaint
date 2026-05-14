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
#include "../history.h"
#include "../layers.h"
#include "../ui/widgets/colorbox.h"
#include "drawing_primitives.h"
#include "tool_options/tool_options.h"
#include <stdlib.h>

/*------------------------------------------------------------------------------
 * Localized Drawing State
 *----------------------------------------------------------------------------*/

static BOOL  s_bDrawing = FALSE;
static POINT s_ptLast = {0};
static int   s_nDrawButton = 0;
static BOOL  s_bPixelsModified = FALSE;
static int   s_activeFreehandTool = 0;

static void MarkAndInvalidateBitmapDirtyRect(const RECT *rcDirtyBitmap) {
  RECT rcDirtyLocal = *rcDirtyBitmap;
  RECT rcScreen;
  LayersMarkDirtyRect(&rcDirtyLocal);
  RectBmpToScr(&rcDirtyLocal, &rcScreen);
  InflateRect(&rcScreen, 2, 2);
  InvalidateCanvasRect(&rcScreen);
}

/*------------------------------------------------------------------------------
 * Drawing Wrappers
 *
 * These wrappers adapt the primitive drawing functions to the specific
 * configuration of the current tool (using global options like nBrushWidth).
 *----------------------------------------------------------------------------*/

static void WrapEraserPoint(BYTE *bits, int width, int height, int x, int y,
                            COLORREF color) {
  DrawPrim_DrawEraserPoint(bits, width, height, x, y, color, nBrushWidth);
}

static void WrapBrushPoint(BYTE *bits, int width, int height, int x, int y,
                           COLORREF color) {
  DrawPrim_DrawBrushPoint(bits, width, height, x, y, color, nBrushWidth);
}

static void WrapBrushLine(BYTE *bits, int width, int height, int x1, int y1,
                          int x2, int y2, COLORREF color) {
  DrawPrim_DrawBrushLine(bits, width, height, x1, y1, x2, y2, color,
                         nBrushWidth);
}

static void WrapSprayPoint(BYTE *bits, int width, int height, int x, int y,
                           COLORREF color) {
  DrawPrim_DrawSprayPoint(bits, width, height, x, y, color, nSprayRadius);
}


/*------------------------------------------------------------------------------
 * Unified Freehand Drawing Engine
 *----------------------------------------------------------------------------*/

typedef void (*PointDrawFn)(BYTE *bits, int width, int height, int x, int y,
                            COLORREF color);
typedef void (*LineDrawFn)(BYTE *bits, int width, int height, int x1, int y1,
                           int x2, int y2, COLORREF color);

typedef struct {
  PointDrawFn pfnPoint;
  LineDrawFn pfnLine;
  BOOL bInterpolate; // Whether to interpolate points between mouse events
} FreehandConfig;

static const FreehandConfig *GetToolConfig(int tool) {
  static const FreehandConfig configs[] = {
      [TOOL_PENCIL] = {DrawPrim_DrawPencilPoint, DrawPrim_DrawPencilLine, TRUE},
      [TOOL_BRUSH] = {WrapBrushPoint, WrapBrushLine, TRUE},
      [TOOL_ERASER] = {WrapEraserPoint, NULL, TRUE},
      [TOOL_AIRBRUSH] = {WrapSprayPoint, NULL, FALSE},
  };

  if (tool >= 0 && tool < sizeof(configs) / sizeof(configs[0])) {
    return &configs[tool];
  }
  return NULL;
}

static void FreehandDrawInterpolated(BYTE *bits, int width, int height,
                                     const FreehandConfig *cfg, int x1, int y1,
                                     int x2, int y2, COLORREF color) {
  // Use the optimized line primitive if available for this configuration
  if (cfg->pfnLine) {
    cfg->pfnLine(bits, width, height, x1, y1, x2, y2, color);
  } else if (cfg->bInterpolate) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = abs(dx) + abs(dy); // Manhattan distance for 4-connectivity

    if (steps == 0) {
      cfg->pfnPoint(bits, width, height, x2, y2, color);
    } else {
      // High-density stepping loop fall-back
      for (int i = 1; i <= steps; i++) {
        int ix = x1 + (dx * i + (steps / 2 * (dx < 0 ? -1 : 1))) / steps;
        int iy = y1 + (dy * i + (steps / 2 * (dy < 0 ? -1 : 1))) / steps;
        cfg->pfnPoint(bits, width, height, ix, iy, color);
      }
    }
  } else {
    cfg->pfnPoint(bits, width, height, x2, y2, color);
  }
}

/*------------------------------------------------------------------------------
 * Unified Event Handlers
 *----------------------------------------------------------------------------*/

static void FreehandOnMouseDown(HWND hWnd, int x, int y, int nButton,
                                int tool) {
  if (s_bDrawing && nButton != s_nDrawButton) {
    CancelFreehandDrawing();
    return;
  }

  const FreehandConfig *cfg = GetToolConfig(tool);
  if (!cfg || !cfg->pfnPoint)
    return;

  s_bDrawing = TRUE;
  s_bPixelsModified = FALSE;
  s_nDrawButton = nButton;
  s_ptLast.x = x;
  s_ptLast.y = y;
  s_activeFreehandTool = tool;
  SetCapture(hWnd);

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    cfg->pfnPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
                  GetColorForButton(nButton));
    LayersMarkDirty();
    s_bPixelsModified = TRUE;

    int radius = DrawPrim_GetBrushSize(nBrushWidth) + 10;
    RECT rcDirty;
    rcDirty.left = x - radius;
    rcDirty.top = y - radius;
    rcDirty.right = x + radius;
    rcDirty.bottom = y + radius;
    MarkAndInvalidateBitmapDirtyRect(&rcDirty);
  }
}

static void FreehandOnMouseMove(HWND hWnd, int x, int y, int nButton,
                                int tool) {
  if (!s_bDrawing || !(nButton & (MK_LBUTTON | MK_RBUTTON)))
    return;
  if (s_activeFreehandTool != tool)
    return;

  const FreehandConfig *cfg = GetToolConfig(tool);
  if (!cfg)
    return;

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    FreehandDrawInterpolated(bits, Canvas_GetWidth(), Canvas_GetHeight(), cfg, s_ptLast.x,
                             s_ptLast.y, x, y, GetColorForButton(s_nDrawButton));

    // Calculate dirty rect in BITMAP space
    int radius = DrawPrim_GetBrushSize(nBrushWidth) + 2;
    RECT rcDirty;
    rcDirty.left = min(s_ptLast.x, x) - radius;
    rcDirty.top = min(s_ptLast.y, y) - radius;
    rcDirty.right = max(s_ptLast.x, x) + radius;
    rcDirty.bottom = max(s_ptLast.y, y) + radius;

    // Send bitmap space to layer engine and invalidate only the changed area.
    MarkAndInvalidateBitmapDirtyRect(&rcDirty);
    s_bPixelsModified = TRUE;
  }
  s_ptLast.x = x;
  s_ptLast.y = y;
  InvalidateCanvas();
}

static void FreehandOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (s_bDrawing && s_bPixelsModified) {
    HistoryPushToolActionById(s_activeFreehandTool, "Draw");
  }
  if (s_bDrawing) {
    s_bDrawing = FALSE;
    ReleaseCapture();
    SetDocumentDirty();
  }
}

/*------------------------------------------------------------------------------
 * Pencil Tool Public API
 *----------------------------------------------------------------------------*/

void PencilToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseDown(hWnd, x, y, nButton, TOOL_PENCIL);
}

void PencilToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseMove(hWnd, x, y, nButton, TOOL_PENCIL);
}

void PencilToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseUp(hWnd, x, y, nButton);
}

/*------------------------------------------------------------------------------
 * Brush Tool Public API
 *----------------------------------------------------------------------------*/

void BrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseDown(hWnd, x, y, nButton, TOOL_BRUSH);
}

void BrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseMove(hWnd, x, y, nButton, TOOL_BRUSH);
}

void BrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseUp(hWnd, x, y, nButton);
}

/*------------------------------------------------------------------------------
 * Eraser Tool Public API
 *----------------------------------------------------------------------------*/

void EraserToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseDown(hWnd, x, y, nButton, TOOL_ERASER);
}

void EraserToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseMove(hWnd, x, y, nButton, TOOL_ERASER);
}

void EraserToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseUp(hWnd, x, y, nButton);
}

/*------------------------------------------------------------------------------
 * Airbrush Tool Public API
 *----------------------------------------------------------------------------*/

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseDown(hWnd, x, y, nButton, TOOL_AIRBRUSH);
  SetTimer(hWnd, 101, 30, NULL);
}

void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  FreehandOnMouseMove(hWnd, x, y, nButton, TOOL_AIRBRUSH);
}

void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  KillTimer(hWnd, 101);
  FreehandOnMouseUp(hWnd, x, y, nButton);
}

void AirbrushToolTrigger(HWND hWnd) {
  if (!s_bDrawing || s_activeFreehandTool != TOOL_AIRBRUSH)
    return;

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawPrim_DrawSprayPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), s_ptLast.x, s_ptLast.y,
                            GetColorForButton(s_nDrawButton), nSprayRadius);
  }
  InvalidateCanvas();
}

/*------------------------------------------------------------------------------
  * Shared State Accessors
  *----------------------------------------------------------------------------*/

static void KillAirbrushTimerIfNeeded(HWND hWnd) {
    if (GetActiveFreehandTool() == TOOL_AIRBRUSH && hWnd) {
        KillTimer(hWnd, 101);
    }
}

BOOL IsFreehandDrawing(void) { return s_bDrawing; }

void FreehandTool_Deactivate(void) {
   if (s_bDrawing) {
     KillAirbrushTimerIfNeeded(GetCanvasWindow());
     s_bDrawing = FALSE;
     ReleaseCapture();
     SetDocumentDirty();
   }
}

void FreehandTool_OnCaptureLost(void) {
   KillAirbrushTimerIfNeeded(GetCanvasWindow());
   if (s_bDrawing && s_bPixelsModified) {
     HistoryPushToolActionById(s_activeFreehandTool, "Draw");
   }
}

BOOL CancelFreehandDrawing(void) {
   BOOL bWasDrawing = s_bDrawing;
   if (s_bDrawing) {
     KillAirbrushTimerIfNeeded(GetCanvasWindow());
     s_bDrawing = FALSE;
     ReleaseCapture();
     InvalidateCanvas();
   }
   return bWasDrawing;
}

int GetActiveFreehandTool(void) { return s_activeFreehandTool; }
