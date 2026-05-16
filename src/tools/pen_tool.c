#include "pen_tool.h"
#include "canvas.h"
#include "draw.h"
#include "helpers.h"
#include "interaction.h"
#include "layers.h"
#include "tool_options/tool_options.h"

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
  {
    int r = (size + 1) / 2;
    if (r < 1)
      r = 1;
    DrawCircleAlpha(bits, width, height, x, y, r, color, alpha,
                    LAYER_BLEND_NORMAL);
  }
}

static void DrawPenLine(BYTE *bits, int width, int height, int x1, int y1,
                        int x2, int y2, COLORREF color, BYTE alpha) {
  int size = GetPenSize();
  if (size <= 1) {
    DrawLineAlpha(bits, width, height, x1, y1, x2, y2, 1, color, alpha,
                  LAYER_BLEND_NORMAL);
    return;
  }
  DrawLineAlpha(bits, width, height, x1, y1, x2, y2, size, color, alpha,
                LAYER_BLEND_NORMAL);
}

void PenTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  Interaction_Begin(hWnd, x, y, nButton, TOOL_PEN);

  BYTE *bits = LayersGetStrokeBits();
  if (!bits) {
    Interaction_Abort();
    return;
  }
  BYTE alpha = GetOpacityForButton(nButton);
  DrawPenPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
               GetColorForButton(nButton), alpha);
  Interaction_MarkModified();
  {
    int pad = GetPenSize() + 2;
    Interaction_NoteStrokeSegment(x, y, x, y, pad);
    Interaction_FlushStrokeRedraw();
  }
}

void PenTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!Interaction_IsActive() || !Interaction_IsActiveButton(nButton))
    return;

  BYTE *bits = LayersGetStrokeBits();
  if (bits) {
    POINT lp;
    Interaction_GetLastPoint(&lp);
    BYTE alpha = GetOpacityForButton(Interaction_GetDrawButton());
    DrawPenLine(bits, Canvas_GetWidth(), Canvas_GetHeight(), lp.x, lp.y, x, y,
                GetColorForButton(Interaction_GetDrawButton()), alpha);
    Interaction_MarkModified();
    Interaction_NoteStrokeSegment(lp.x, lp.y, x, y, GetPenSize() + 2);
  }
  Interaction_UpdateLastPoint(x, y);
}

void PenTool_OnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  Interaction_Commit("Draw");
}

BOOL IsPenDrawing(void) {
  return Interaction_IsActive() && Interaction_GetActiveToolId() == TOOL_PEN;
}

void PenTool_Deactivate(void) {
  if (Interaction_IsActive() && Interaction_GetActiveToolId() == TOOL_PEN)
    Interaction_EndQuiet();
}

BOOL PenTool_Cancel(void) {
  if (!Interaction_IsActive() || Interaction_GetActiveToolId() != TOOL_PEN)
    return FALSE;
  Interaction_Abort();
  return TRUE;
}
