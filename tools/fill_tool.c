#include "fill_tool.h"
#include "../peztold_core.h"

#include "../floodfill.h"
#include "../helpers.h"
#include "../history.h"

typedef struct FillRequest {
  int x;
  int y;
  int button;
} FillRequest;

static BOOL FillRequest_IsSupportedButton(const FillRequest *request) {
  return request && (request->button == MK_LBUTTON || request->button == MK_RBUTTON);
}

static BOOL FillTool_Execute(const FillRequest *request) {
  HBITMAP hOldColor = NULL;
  HDC hColor = LayersGetActiveColorDC(&hOldColor);
  if (!hColor) return FALSE;

  BOOL changed = FloodFillCanvas(request->x, request->y,
                                 GetColorForButton(request->button), 255);
  ReleaseBitmapDC(hColor, hOldColor);
  return changed;
}

static void FillTool_CommitResult(BOOL changed) {
  if (!changed) return;
  HistoryPushToolActionById(TOOL_FILL, "Flood Fill");
  InvalidateCanvas();
  SetDocumentDirty();
}

void FillToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  FillRequest request = {.x = x, .y = y, .button = nButton};
  if (!FillRequest_IsSupportedButton(&request)) return;
  FillTool_CommitResult(FillTool_Execute(&request));
}
