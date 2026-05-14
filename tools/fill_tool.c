#include "fill_tool.h"
#include "../peztold_core.h"

#include "../floodfill.h"
#include "../helpers.h"
#include "../history.h"
#include "../layers.h"

void FillToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (nButton != MK_LBUTTON && nButton != MK_RBUTTON) {
    return;
  }

  HBITMAP hOldColor = NULL;
  HDC hColor = LayersGetActiveColorDC(&hOldColor);
  if (hColor) {
    BOOL changed = FloodFillCanvas(x, y, GetColorForButton(nButton), 255);
    ReleaseBitmapDC(hColor, hOldColor);
    if (changed) {
      HistoryPushToolActionById(TOOL_FILL, "Flood Fill");
      InvalidateCanvas();
      SetDocumentDirty();
    }
  }
}
