#include "interaction.h"
#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"

static struct { BOOL active, modified, capture; int btn, tool, lx, ly; HWND hWnd; } ix;

static void EndState(void) { ix.active = ix.modified = FALSE; ix.capture = TRUE; ix.hWnd = NULL; ReleaseCapture(); }

void Interaction_BeginEx(HWND hWnd, int x, int y, int btn, int tool, BOOL capture) {
    Layers_BeginWrite();
    ix.active = TRUE; ix.modified = FALSE; ix.btn = btn; ix.tool = tool;
    ix.lx = x; ix.ly = y; ix.hWnd = hWnd; ix.capture = capture;
    if (hWnd && capture) SetCapture(hWnd);
}

void Interaction_Begin(HWND hWnd, int x, int y, int btn, int tool) { Interaction_BeginEx(hWnd, x, y, btn, tool, TRUE); }
void Interaction_UpdateLastPoint(int x, int y) { ix.lx = x; ix.ly = y; }
void Interaction_MarkModified(void) { ix.modified = TRUE; }
BOOL Interaction_IsActive(void) { return ix.active; }
BOOL Interaction_IsModified(void) { return ix.modified; }

BOOL Interaction_Commit(const char *label) {
    if (!ix.active) return FALSE;
    BOOL draft = LayersIsDraftDirty();
    if (draft) LayersMergeDraftToActive();
    if (draft || ix.modified) LayersMarkDirty();
    if (ix.modified || draft) HistoryPushToolActionById(ix.tool, label ? label : "Draw");
    else Core_Notify(EV_PIXELS_CHANGED);
    EndState(); SetDocumentDirty(); InvalidateCanvas(); History_ClearPendingLayerSnapshot();
    return TRUE;
}

void Interaction_Abort(void) {
    if (!ix.active) return;
    LayersClearDraft(); History_AbortPendingLayerWrite(); EndState(); InvalidateCanvas(); Core_Notify(EV_PIXELS_CHANGED);
}

void Interaction_EndQuiet(void) {
    if (!ix.active) return;
    EndState(); SetDocumentDirty(); InvalidateCanvas(); History_ClearPendingLayerSnapshot();
}

BOOL Interaction_IsActiveButton(int btn) { return ix.active && (btn & ix.btn) != 0; }

void Interaction_OnCaptureLost(const char *label) {
    if (!ix.active) return;
    if (ix.modified) HistoryPushToolActionById(ix.tool, label ? label : "Draw");
    else Core_Notify(EV_PIXELS_CHANGED);
    EndState(); SetDocumentDirty(); InvalidateCanvas(); History_ClearPendingLayerSnapshot();
}

int Interaction_GetActiveToolId(void) { return ix.active ? ix.tool : -1; }
int Interaction_GetDrawButton(void) { return ix.btn; }
void Interaction_GetLastPoint(POINT *out) { if (out) { out->x = ix.lx; out->y = ix.ly; } }