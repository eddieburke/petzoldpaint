#include "fill_tool.h"
#include "../peztold_core.h"

#include "../floodfill.h"
#include "../helpers.h"
#include "../history.h"

void FillToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;

  if (nButton != MK_LBUTTON && nButton != MK_RBUTTON)
    return;

  if (!FloodFillCanvas(x, y, GetColorForButton(nButton), 255))
    return;

  HistoryPushToolActionById(TOOL_FILL, "Flood Fill");
  InvalidateCanvas();
  SetDocumentDirty();
}
