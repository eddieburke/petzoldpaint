#include "interaction.h"
#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"

static struct {
  BOOL active;
  BOOL modified;
  BOOL capture;
  int btn;
  int toolId;
  int lx;
  int ly;
  HWND hWnd;
} ix;

static void EndState(void) {
  ix.active = FALSE;
  ix.modified = FALSE;
  ix.capture = TRUE;
  ix.hWnd = NULL;
  ReleaseCapture();
}

void Interaction_BeginEx(HWND hWnd, int x, int y, int btn, int tool, BOOL capture) {
  Layers_BeginWrite();
  ix.active = TRUE;
  ix.modified = FALSE;
  ix.btn = btn;
  ix.toolId = tool;
  ix.lx = x;
  ix.ly = y;
  ix.hWnd = hWnd;
  ix.capture = capture;
  if (hWnd && capture) {
    SetCapture(hWnd);
  }
}

void Interaction_Begin(HWND hWnd, int x, int y, int btn, int tool) {
  Interaction_BeginEx(hWnd, x, y, btn, tool, TRUE);
}

void Interaction_UpdateLastPoint(int x, int y) {
  ix.lx = x;
  ix.ly = y;
}

void Interaction_MarkModified(void) { ix.modified = TRUE; }
BOOL Interaction_IsActive(void) { return ix.active; }
BOOL Interaction_IsModified(void) { return ix.modified; }

BOOL Interaction_Commit(const char *label) {
  if (!ix.active) return FALSE;
  BOOL draft = LayersIsDraftDirty();
  if (draft) LayersMergeDraftToActive();
  if (draft || ix.modified) {
    LayersMarkDirty();
  }
  if (ix.modified || draft) {
    HistoryPush(label ? label : "Draw");
  } else {
    Core_Notify(EV_PIXELS_CHANGED);
  }
  EndState();
  SetDocumentDirty();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  return TRUE;
}

void Interaction_Abort(void) {
  if (!ix.active) return;
  LayersClearDraft();
  EndState();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  Core_Notify(EV_PIXELS_CHANGED);
}

void Interaction_EndQuiet(void) {
  if (!ix.active) return;
  BOOL modified = ix.modified;
  EndState();
  if (modified) {
    SetDocumentDirty();
  }
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

BOOL Interaction_IsActiveButton(int btn) { return ix.active && (btn & ix.btn) != 0; }

void Interaction_OnCaptureLost(const char *label) {
  if (!ix.active) return;
  BOOL draft = LayersIsDraftDirty();
  if (draft) LayersMergeDraftToActive();
  if (draft || ix.modified) {
    LayersMarkDirty();
    HistoryPush(label ? label : "Draw");
  } else {
    Core_Notify(EV_PIXELS_CHANGED);
  }
  EndState();
  SetDocumentDirty();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

int Interaction_GetActiveToolId(void) { return ix.toolId; }
int Interaction_GetDrawButton(void) { return ix.btn; }
void Interaction_GetLastPoint(POINT *out) {
  if (!out) return;
  out->x = ix.lx;
  out->y = ix.ly;
}
