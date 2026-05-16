#include "interaction.h"
#include "canvas.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
static RECT s_strokeDirty;
static BOOL s_strokeDirtyValid;
static void StrokeDirtyReset(void) {
	s_strokeDirtyValid = FALSE;
}
static void StrokeDirtyUnionPoint(int x, int y, int pad) {
	RECT pt = {x - pad, y - pad, x + pad + 1, y + pad + 1};
	if (!s_strokeDirtyValid) {
		s_strokeDirty = pt;
		s_strokeDirtyValid = TRUE;
		return;
	}
	if (pt.left < s_strokeDirty.left)
		s_strokeDirty.left = pt.left;
	if (pt.top < s_strokeDirty.top)
		s_strokeDirty.top = pt.top;
	if (pt.right > s_strokeDirty.right)
		s_strokeDirty.right = pt.right;
	if (pt.bottom > s_strokeDirty.bottom)
		s_strokeDirty.bottom = pt.bottom;
}
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
void Interaction_NoteStrokeSegment(int x1, int y1, int x2, int y2, int padBmp) {
	if (!ix.active)
		return;
	if (padBmp < 0)
		padBmp = 0;
	StrokeDirtyUnionPoint(x1, y1, padBmp);
	StrokeDirtyUnionPoint(x2, y2, padBmp);
}
void Interaction_FlushStrokeRedraw(void) {
	if (!s_strokeDirtyValid)
		return;
	LayersMarkDirtyRect(s_strokeDirty.left, s_strokeDirty.top, s_strokeDirty.right, s_strokeDirty.bottom);
	Canvas_InvalidateBitmapRect(&s_strokeDirty);
	s_strokeDirtyValid = FALSE;
}
static void EndState(void) {
	BOOL hadCapture = ix.capture;
	ix.active = FALSE;
	ix.modified = FALSE;
	ix.capture = FALSE;
	ix.hWnd = NULL;
	if (hadCapture)
		ReleaseCapture();
}
BOOL Interaction_BeginEx(HWND hWnd, int x, int y, int btn, int tool, BOOL capture) {
	if (ix.active)
		return FALSE;
	StrokeDirtyReset();
	LayersBeginStroke();
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
	return TRUE;
}
BOOL Interaction_Begin(HWND hWnd, int x, int y, int btn, int tool) {
	return Interaction_BeginEx(hWnd, x, y, btn, tool, TRUE);
}
void Interaction_UpdateLastPoint(int x, int y) {
	ix.lx = x;
	ix.ly = y;
}
void Interaction_MarkModified(void) {
	ix.modified = TRUE;
	LayersMarkDraftDirty();
}
BOOL Interaction_IsActive(void) {
	return ix.active;
}
BOOL Interaction_IsModified(void) {
	return ix.modified;
}
static void InvalidateAfterInteraction(BOOL changed) {
	if (!changed)
		return;
	if (s_strokeDirtyValid)
		Canvas_InvalidateBitmapRect(&s_strokeDirty);
	else
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
static BOOL FinalizeInteraction(const char *pushHistory) {
	BOOL draft = LayersIsDraftDirty();
	BOOL changed = ix.modified || draft;
	if (draft) {
		if (s_strokeDirtyValid)
			LayersMergeDraftRect(&s_strokeDirty);
		else
			LayersMergeDraftToActive();
	} else if (changed) {
		LayersMarkDirty();
	}
	if (changed) {
		if (pushHistory)
			HistoryPush(pushHistory);
		else
			Core_Notify(EV_PIXELS_CHANGED);
	}
	EndState();
	if (changed)
		SetDocumentDirty();
	InvalidateAfterInteraction(changed);
	return changed;
}
BOOL Interaction_Commit(const char *label) {
	if (!ix.active)
		return FALSE;
	FinalizeInteraction(label ? label : "Draw");
	return TRUE;
}
void Interaction_Abort(void) {
	if (!ix.active)
		return;
	LayersClearDraft();
	EndState();
	if (s_strokeDirtyValid)
		Canvas_InvalidateBitmapRect(&s_strokeDirty);
	else
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	Core_Notify(EV_PIXELS_CHANGED);
}
void Interaction_EndQuiet(void) {
	if (!ix.active)
		return;
	FinalizeInteraction(NULL);
}
BOOL Interaction_IsActiveButton(int btn) {
	return ix.active && (btn & ix.btn) != 0;
}
void Interaction_OnCaptureLost(const char *label) {
	if (!ix.active)
		return;
	FinalizeInteraction(label ? label : "Draw");
}
int Interaction_GetActiveToolId(void) {
	return ix.toolId;
}
int Interaction_GetDrawButton(void) {
	return ix.btn;
}
void Interaction_GetLastPoint(POINT *out) {
	if (!out)
		return;
	out->x = ix.lx;
	out->y = ix.ly;
}
