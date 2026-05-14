#include "fill_tool.h"
#include "../peztold_core.h"

#include "../floodfill.h"
#include "../helpers.h"
#include "../history.h"
#include "../layers.h"

void FillToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  HBITMAP hOldColor = NULL;
  HDC hColor = LayersGetActiveColorDC(&hOldColor);
  if (hColor) {
    FloodFillCanvas(x, y, GetColorForButton(nButton), 255);
    ReleaseBitmapDC(hColor, hOldColor);
    HistoryPushToolActionById(TOOL_FILL, "Flood Fill");
  }
  InvalidateCanvas();
  SetDocumentDirty();
}
