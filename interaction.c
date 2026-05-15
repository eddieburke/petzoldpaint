#include "interaction.h"
#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"

static struct {
  BOOL isDrawing;
  BOOL pixelsModified;
  int drawButton;
  int toolId;
  POINT lastPoint;
  HWND capWnd;
  BOOL useCapture;
} s_ix;

void Interaction_BeginEx(HWND hWnd, int x, int y, int nButton, int toolId,
                         BOOL captureMouse) {
  Layers_BeginWrite();
  s_ix.isDrawing = TRUE;
  s_ix.pixelsModified = FALSE;
  s_ix.drawButton = nButton;
  s_ix.toolId = toolId;
  s_ix.lastPoint.x = x;
  s_ix.lastPoint.y = y;
  s_ix.capWnd = hWnd;
  s_ix.useCapture = captureMouse;
  if (hWnd && captureMouse)
    SetCapture(hWnd);
}

void Interaction_Begin(HWND hWnd, int x, int y, int nButton, int toolId) {
  Interaction_BeginEx(hWnd, x, y, nButton, toolId, TRUE);
}

void Interaction_UpdateLastPoint(int x, int y) {
  s_ix.lastPoint.x = x;
  s_ix.lastPoint.y = y;
}

void Interaction_MarkModified(void) { s_ix.pixelsModified = TRUE; }

BOOL Interaction_IsActive(void) { return s_ix.isDrawing; }

BOOL Interaction_IsModified(void) { return s_ix.pixelsModified; }

BOOL Interaction_Commit(const char *label) {
  if (!s_ix.isDrawing)
    return FALSE;
  BOOL hadDraft = LayersIsDraftDirty();
  if (hadDraft)
    LayersMergeDraftToActive();
  if (hadDraft || s_ix.pixelsModified)
    LayersMarkDirty();
  if (s_ix.pixelsModified || hadDraft)
    HistoryPushToolActionById(s_ix.toolId, label ? label : "Draw");
  else
    Core_Notify(EV_PIXELS_CHANGED);
  s_ix.isDrawing = FALSE;
  s_ix.pixelsModified = FALSE;
  s_ix.capWnd = NULL;
  s_ix.useCapture = TRUE;
  ReleaseCapture();
  SetDocumentDirty();
  InvalidateCanvas();
  History_ClearPendingLayerSnapshot();
  return TRUE;
}

void Interaction_Abort(void) {
  if (!s_ix.isDrawing)
    return;
  LayersClearDraft();
  History_AbortPendingLayerWrite();
  s_ix.isDrawing = FALSE;
  s_ix.pixelsModified = FALSE;
  s_ix.capWnd = NULL;
  s_ix.useCapture = TRUE;
  ReleaseCapture();
  InvalidateCanvas();
  Core_Notify(EV_PIXELS_CHANGED);
}

void Interaction_EndQuiet(void) {
  if (!s_ix.isDrawing)
    return;
  s_ix.isDrawing = FALSE;
  s_ix.pixelsModified = FALSE;
  s_ix.capWnd = NULL;
  s_ix.useCapture = TRUE;
  ReleaseCapture();
  SetDocumentDirty();
  InvalidateCanvas();
  History_ClearPendingLayerSnapshot();
}

BOOL Interaction_IsActiveButton(int nButton) {
  return (nButton & (MK_LBUTTON | MK_RBUTTON)) != 0;
}

void Interaction_OnCaptureLost(const char *defaultLabel) {
  if (!s_ix.isDrawing)
    return;
  if (s_ix.pixelsModified)
    HistoryPushToolActionById(s_ix.toolId, defaultLabel ? defaultLabel : "Draw");
  else
    Core_Notify(EV_PIXELS_CHANGED);
  s_ix.isDrawing = FALSE;
  s_ix.pixelsModified = FALSE;
  s_ix.capWnd = NULL;
  s_ix.useCapture = TRUE;
  SetDocumentDirty();
  InvalidateCanvas();
  History_ClearPendingLayerSnapshot();
}

int Interaction_GetActiveToolId(void) {
  return s_ix.isDrawing ? s_ix.toolId : -1;
}

int Interaction_GetDrawButton(void) { return s_ix.drawButton; }

void Interaction_GetLastPoint(POINT *outPt) {
  if (outPt)
    *outPt = s_ix.lastPoint;
}
