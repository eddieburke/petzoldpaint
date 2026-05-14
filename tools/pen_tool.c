/*------------------------------------------------------------------------------
 * PEN_TOOL.C
 *
 * Ballpoint Pen Tool Implementation
 *
 * Implements the ballpoint pen tool, which draws smooth lines with
 * anti-aliasing to simulate ink.
 *----------------------------------------------------------------------------*/

#include "pen_tool.h"
#include "../canvas.h"
#include "../draw.h"
#include "../helpers.h"
#include "../layers.h"
#include "tool_options/tool_options.h"
#include <math.h>
#include "stroke_session.h"

static StrokeSession s_session = {0};

static int GetPenSize(void) {
   int sizes[] = {1, 2, 3, 4, 5};
   int idx = (nBrushWidth - 1) % 5;
   if (idx < 0) idx = 0;
   if (idx >= (int)(sizeof(sizes) / sizeof(sizes[0]))) idx = 0;
   return sizes[idx];
}

static void DrawPenPoint(BYTE *bits, int width, int height, int x, int y,
                          COLORREF color) {
  int size = GetPenSize();
  if (size <= 1) {
    DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    return;
  }
  DrawCircleAAAlpha(bits, width, height, x, y, size / 2.0f, color, 255, LAYER_BLEND_NORMAL);
}
 
static void DrawPenLine(BYTE *bits, int width, int height, int x1, int y1,
                        int x2, int y2, COLORREF color) {
  int size = GetPenSize();
  if (size <= 1) {
    DrawLineAlpha(bits, width, height, x1, y1, x2, y2, 1, color, 255, LAYER_BLEND_NORMAL);
    return;
  }
  DrawLineAAAlpha(bits, width, height, (float)x1, (float)y1, (float)x2,
                  (float)y2, size / 2.0f, color, 255, LAYER_BLEND_NORMAL);
}

void PenToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  StrokeSession_Begin(&s_session, hWnd, x, y, nButton, TOOL_PEN);

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawPenPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
                 GetColorForButton(nButton));
    LayersMarkDirty();
    StrokeSession_MarkPixelsModified(&s_session);
  }
  InvalidateCanvas();
}

void PenToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!s_session.isDrawing || !StrokeSession_IsActiveButton(nButton))
    return;

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawPenLine(bits, Canvas_GetWidth(), Canvas_GetHeight(), s_session.lastPoint.x,
                s_session.lastPoint.y, x, y, GetColorForButton(s_session.drawButton));
    LayersMarkDirty();
    StrokeSession_MarkPixelsModified(&s_session);
  }
  StrokeSession_UpdateLastPoint(&s_session, x, y);
  InvalidateCanvas();
}

void PenToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  StrokeSession_CommitIfNeeded(&s_session, "Draw");
  StrokeSession_End(&s_session);
}

BOOL IsPenDrawing(void) { return s_session.isDrawing; }

void PenTool_Deactivate(void) {
  StrokeSession_End(&s_session);
}

BOOL CancelPenDrawing(void) {
  if (!s_session.isDrawing) return FALSE;
  StrokeSession_Cancel(&s_session);
  return TRUE;
}
