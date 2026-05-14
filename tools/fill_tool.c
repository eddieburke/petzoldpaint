#include "fill_tool.h"
#include "../peztold_core.h"

#include "../floodfill.h"
#include "../helpers.h"
#include "../history.h"

void FillToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  BOOL changed = FloodFillCanvas(x, y, GetColorForButton(nButton), 255);
  if (changed) {
    HistoryPushToolActionById(TOOL_FILL, "Flood Fill");
    InvalidateCanvas();
    SetDocumentDirty();
  }
}
