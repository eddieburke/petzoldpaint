#include "fill_tool.h"

#include "peztold_core.h"
#include "floodfill.h"
#include "helpers.h"
#include "history.h"
#include "canvas.h"

void FillTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (nButton != MK_LBUTTON && nButton != MK_RBUTTON)
    return;

  if (FloodFillCanvas(x, y, GetColorForButton(nButton),
                      GetOpacityForButton(nButton))) {
    HistoryPush("Flood Fill");
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    SetDocumentDirty();
  }
}
