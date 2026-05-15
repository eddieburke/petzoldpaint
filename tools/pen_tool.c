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
#include "../interaction.h"
#include "../layers.h"
#include "tool_options/tool_options.h"
#include <math.h>

static int GetPenSize(void) {
  int sizes[] = {1, 2, 3, 4, 5};
  int idx = (nBrushWidth - 1) % 5;
  if (idx < 0)
    idx = 0;
  if (idx >= (int)(sizeof(sizes) / sizeof(sizes[0])))
    idx = 0;
  return sizes[idx];
}

static void DrawPenPoint(BYTE *bits, int width, int height, int x, int y,
                         COLORREF color, BYTE alpha) {
  int size = GetPenSize();
  if (size <= 1) {
    DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    return;
  }
  DrawCircleAAAlpha(bits, width, height, x, y, size / 2.0f, color, alpha,
                    LAYER_BLEND_NORMAL);
}

static void DrawPenLine(BYTE *bits, int width, int height, int x1, int y1,
                        int x2, int y2, COLORREF color, BYTE alpha) {
  int size = GetPenSize();
  if (size <= 1) {
    DrawLineAlpha(bits, width, height, x1, y1, x2, y2, 1, color, alpha,
                  LAYER_BLEND_NORMAL);
    return;
  }
  DrawLineAAAlpha(bits, width, height, (float)x1, (float)y1, (float)x2,
                  (float)y2, size / 2.0f, color, alpha, LAYER_BLEND_NORMAL);
}

void PenToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  Interaction_Begin(hWnd, x, y, nButton, TOOL_PEN);

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    BYTE alpha = GetOpacityForButton(nButton);
    DrawPenPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
                 GetColorForButton(nButton), alpha);
    LayersMarkDirty();
    Interaction_MarkModified();
  }
  InvalidateCanvas();
}

void PenToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!Interaction_IsActive() || !Interaction_IsActiveButton(nButton))
    return;

  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    POINT lp;
    Interaction_GetLastPoint(&lp);
    BYTE alpha = GetOpacityForButton(Interaction_GetDrawButton());
    DrawPenLine(bits, Canvas_GetWidth(), Canvas_GetHeight(), lp.x, lp.y, x, y,
                GetColorForButton(Interaction_GetDrawButton()), alpha);
    LayersMarkDirty();
    Interaction_MarkModified();
  }
  Interaction_UpdateLastPoint(x, y);
  InvalidateCanvas();
}

void PenToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  Interaction_Commit("Draw");
}

BOOL IsPenDrawing(void) { return Interaction_IsActive(); }

void PenTool_Deactivate(void) { Interaction_EndQuiet(); }

BOOL CancelPenDrawing(void) {
  if (!Interaction_IsActive())
    return FALSE;
  Interaction_Abort();
  return TRUE;
}
