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

static BOOL  s_bDrawing = FALSE;
static POINT s_ptLast = {0};
static int   s_nDrawButton = 0;
static BOOL  s_bPixelsModified = FALSE;

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
  s_bDrawing = TRUE;
  s_bPixelsModified = FALSE;
  s_nDrawButton = nButton;
  s_ptLast.x = x;
  s_ptLast.y = y;
  SetCapture(hWnd);

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawPenPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
                 GetColorForButton(nButton));
    LayersMarkDirty();
    s_bPixelsModified = TRUE;
  }
  InvalidateCanvas();
}

void PenToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (!s_bDrawing || !(nButton & (MK_LBUTTON | MK_RBUTTON)))
    return;

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawPenLine(bits, Canvas_GetWidth(), Canvas_GetHeight(), s_ptLast.x,
                s_ptLast.y, x, y, GetColorForButton(s_nDrawButton));
    LayersMarkDirty();
    s_bPixelsModified = TRUE;
  }
  s_ptLast.x = x;
  s_ptLast.y = y;
  InvalidateCanvas();
}

void PenToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (s_bDrawing && s_bPixelsModified) {
    HistoryPushToolActionById(TOOL_PEN, "Draw");
  }
  if (s_bDrawing) {
    s_bDrawing = FALSE;
    ReleaseCapture();
    SetDocumentDirty();
  }
}

void PenTool_OnCaptureLost(void)
{
     if (s_bDrawing && s_bPixelsModified) {
         HistoryPushToolActionById(TOOL_PEN, "Draw");
     }
}

BOOL IsPenDrawing(void) { return s_bDrawing; }

void PenTool_Deactivate(void) {
  if (s_bDrawing) {
    s_bDrawing = FALSE;
    ReleaseCapture();
    SetDocumentDirty();
  }
}

BOOL CancelPenDrawing(void) {
  if (!s_bDrawing) return FALSE;
  s_bDrawing = FALSE;
  ReleaseCapture();
  InvalidateCanvas();
  return TRUE;
}
