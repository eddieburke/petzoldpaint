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

static void DrawInterpolated(BYTE *bits, int width, int height,
                             const FreehandToolDef *tool, int x1, int y1,
                             int x2, int y2, COLORREF color) {
  int size = tool->size();

  if (tool->line) {
    tool->line(bits, width, height, x1, y1, x2, y2, color, size);
    return;
  }

  int dx = x2 - x1;
  int dy = y2 - y1;
  int steps = abs(dx) + abs(dy);
  if (steps == 0) {
    tool->point(bits, width, height, x2, y2, color, size);
    return;
  }

  for (int i = 1; i <= steps; ++i) {
    int ix = x1 + (dx * i) / steps;
    int iy = y1 + (dy * i) / steps;
    tool->point(bits, width, height, ix, iy, color, size);
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

  BYTE *bits = LayersGetActiveColorBits();
  if (!bits)
    return;

  tool->point(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
              ToolColor(tool, nButton), tool->size());
  LayersMarkDirty();
  MarkDirtyAroundLine(tool, x, y, x, y);
  StrokeSession_MarkPixelsModified(&s_session);
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

  DrawInterpolated(bits, Canvas_GetWidth(), Canvas_GetHeight(), tool,
                   s_session.lastPoint.x, s_session.lastPoint.y, x, y,
                   ToolColor(tool, s_session.drawButton));
  MarkDirtyAroundLine(tool, s_session.lastPoint.x, s_session.lastPoint.y, x, y);
  StrokeSession_MarkPixelsModified(&s_session);
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
