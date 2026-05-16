#include "peztold_core.h"
#include "canvas.h"
#include "controller.h"
#include "geom.h"
#include "interaction.h"
#include "tools.h"
#include "tool_session.h"
#include "tools/bezier_tool.h"
#include "tools/crayon_tool.h"
#include "tools/fill_tool.h"
#include "tools/freehand_tools.h"
#include "tools/highlighter_tool.h"
#include "tools/magnifier_tool.h"
#include "tools/pen_tool.h"
#include "tools/pick_tool.h"
#include "tools/polygon_tool.h"
#include "tools/selection_tool.h"
#include "tools/shape_tools.h"
#include "tools/text_tool.h"
#include "tools/tool_options/presets.h"
#include "tools/tool_options/tool_options.h"
#include "ui/widgets/toolbar.h"
#include <assert.h>
#include <stdlib.h>
typedef struct ToolRuntimeState {
	int currentTool;
	int pointerTool;
	HWND captureOwner;
	BOOL pointerActive;
} ToolRuntimeState;
static ToolRuntimeState s_runtime = {TOOL_PENCIL, -1, NULL, FALSE};
BOOL Tool_IsValidId(int toolId) {
	return toolId >= 0 && toolId < NUM_TOOLS;
}
static void Tool_DebugInvalidId(int toolId) {
#ifdef _DEBUG
	char msg[96];
	StringCchPrintfA(msg, sizeof(msg), "Invalid tool id: %d\n", toolId);
	OutputDebugStringA(msg);
	assert(FALSE);
#else
	(void)toolId;
#endif
}
static BOOL Tool_IsSelectionTool(int toolId) {
	return toolId == TOOL_SELECT || toolId == TOOL_FREEFORM;
}
static BOOL Tool_IsShapeTool(int toolId) {
	return toolId == TOOL_LINE || toolId == TOOL_RECT || toolId == TOOL_ELLIPSE || toolId == TOOL_ROUNDRECT;
}
static BOOL Tool_IsStrokeTool(int toolId) {
	return toolId == TOOL_ERASER || toolId == TOOL_PENCIL || toolId == TOOL_BRUSH || toolId == TOOL_AIRBRUSH || toolId == TOOL_PEN || toolId == TOOL_HIGHLIGHTER || toolId == TOOL_CRAYON;
}
static BOOL Tool_IsModalTool(int toolId) {
	return Tool_IsShapeTool(toolId) || toolId == TOOL_CURVE || toolId == TOOL_POLYGON || toolId == TOOL_TEXT;
}
static const char *Tool_CaptureLostHistoryLabel(int toolId) {
	if (Tool_IsShapeTool(toolId))
		return "Draw Shape";
	switch (toolId) {
	case TOOL_CURVE:
		return "Draw Curve";
	case TOOL_POLYGON:
		return "Draw Polygon";
	default:
		return "Draw";
	}
}
static void Tool_ClearPointerRuntime(void) {
	s_runtime.pointerTool = -1;
	s_runtime.captureOwner = NULL;
	s_runtime.pointerActive = FALSE;
}
static BOOL Tool_IsToolBusy(int toolId) {
	if (!Tool_IsValidId(toolId))
		return FALSE;
	if (Tool_IsSelectionTool(toolId))
		return SelectionIsDragging();
	if (Tool_IsShapeTool(toolId))
		return IsShapePending();
	switch (toolId) {
	case TOOL_ERASER:
	case TOOL_PENCIL:
	case TOOL_BRUSH:
	case TOOL_AIRBRUSH:
		return IsFreehandDrawing();
	case TOOL_TEXT:
		return IsTextEditing();
	case TOOL_CURVE:
		return IsCurvePending();
	case TOOL_POLYGON:
		return IsPolygonPending();
	case TOOL_PEN:
		return IsPenDrawing();
	case TOOL_HIGHLIGHTER:
		return IsHighlighterDrawing();
	case TOOL_CRAYON:
		return IsCrayonDrawing();
	default:
		return FALSE;
	}
}
BOOL Tool_IsCurrentBusy(void) {
	return Tool_IsToolBusy(Tool_GetCurrent());
}
static int ResolveActiveToolForMouseDown(void) {
	return (IsAltDown() && GetCapture() == NULL) ? TOOL_PICK : Tool_GetCurrent();
}
static int ResolveActiveToolForMoveUp(void) {
	if (s_runtime.pointerActive && Tool_IsValidId(s_runtime.pointerTool))
		return s_runtime.pointerTool;
	if (IsAltDown() && !Tool_IsToolBusy(Tool_GetCurrent()))
		return TOOL_PICK;
	return Tool_GetCurrent();
}
static void Tool_DispatchPointer(const ToolPointerEvent *ev, int toolId) {
	if (!Tool_IsValidId(toolId)) {
		Tool_DebugInvalidId(toolId);
		return;
	}
	if (Tool_IsSelectionTool(toolId)) {
		SelectionTool_OnPointer(ev);
		return;
	}
	if (Tool_IsShapeTool(toolId)) {
		ShapeTool_OnPointer(ev);
		return;
	}
	switch (toolId) {
	case TOOL_ERASER:
	case TOOL_PENCIL:
	case TOOL_BRUSH:
		FreehandTool_OnPointer(ev);
		break;
	case TOOL_AIRBRUSH:
		AirbrushTool_OnPointer(ev);
		break;
	case TOOL_FILL:
		FillTool_OnPointer(ev);
		break;
	case TOOL_PICK:
		PickTool_OnPointer(ev);
		break;
	case TOOL_MAGNIFIER:
		MagnifierTool_OnPointer(ev);
		break;
	case TOOL_TEXT:
		TextTool_OnPointer(ev);
		break;
	case TOOL_CURVE:
		BezierTool_OnPointer(ev);
		break;
	case TOOL_POLYGON:
		PolygonTool_OnPointer(ev);
		break;
	case TOOL_PEN:
		PenTool_OnPointer(ev);
		break;
	case TOOL_HIGHLIGHTER:
		HighlighterTool_OnPointer(ev);
		break;
	case TOOL_CRAYON:
		CrayonTool_OnPointer(ev);
		break;
	default:
		break;
	}
}
BOOL ToolHandleOverlayPointerEvent(HWND hWnd, int screenX, int screenY, int nButton) {
	int toolId = Tool_GetCurrent();
	if (Tool_IsSelectionTool(toolId))
		return SelectionTool_HandleOverlayClick(hWnd, screenX, screenY, nButton);
	if (Tool_IsShapeTool(toolId))
		return ShapeTool_HandleOverlayClick(hWnd, screenX, screenY, nButton);
	switch (toolId) {
	case TOOL_TEXT:
		return TextTool_HandleOverlayClick(hWnd, screenX, screenY, nButton);
	case TOOL_CURVE:
		return BezierTool_HandleOverlayClick(hWnd, screenX, screenY, nButton);
	case TOOL_POLYGON:
		return PolygonTool_HandleOverlayClick(hWnd, screenX, screenY, nButton);
	default:
		return FALSE;
	}
}
static void Tool_DrawOverlayForCurrent(const OverlayContext *ctx) {
	int toolId = Tool_GetCurrent();
	if (Tool_IsSelectionTool(toolId)) {
		SelectionTool_DrawOverlay(ctx);
		return;
	}
	if (Tool_IsShapeTool(toolId)) {
		ShapeTool_DrawOverlay(ctx);
		return;
	}
	switch (toolId) {
	case TOOL_MAGNIFIER:
		MagnifierTool_DrawOverlay(ctx);
		break;
	case TOOL_TEXT:
		TextTool_DrawOverlay(ctx);
		break;
	case TOOL_CURVE:
		BezierTool_DrawOverlay(ctx);
		break;
	case TOOL_POLYGON:
		PolygonTool_DrawOverlay(ctx);
		break;
	default:
		break;
	}
}
static void Tool_CancelTool(int toolId, ToolCancelReason reason) {
	if (!Tool_IsValidId(toolId))
		return;
	if (Tool_IsSelectionTool(toolId)) {
		SelectionTool_Cancel(reason);
		return;
	}
	if (Tool_IsShapeTool(toolId)) {
		ShapeTool_Cancel();
		return;
	}
	switch (toolId) {
	case TOOL_ERASER:
	case TOOL_PENCIL:
	case TOOL_BRUSH:
	case TOOL_AIRBRUSH:
		FreehandTool_Cancel();
		break;
	case TOOL_MAGNIFIER:
		MagnifierTool_Deactivate();
		break;
	case TOOL_TEXT:
		TextTool_Cancel();
		break;
	case TOOL_CURVE:
		BezierTool_Cancel();
		break;
	case TOOL_POLYGON:
		PolygonTool_Cancel();
		break;
	case TOOL_PEN:
		PenTool_Cancel();
		break;
	case TOOL_HIGHLIGHTER:
		HighlighterTool_Cancel();
		break;
	case TOOL_CRAYON:
		CrayonTool_Cancel();
		break;
	default:
		break;
	}
}
static void Tool_DeactivateTool(int toolId) {
	if (!Tool_IsValidId(toolId))
		return;
	if (Tool_IsSelectionTool(toolId)) {
		SelectionTool_Deactivate();
		return;
	}
	if (Tool_IsShapeTool(toolId)) {
		ShapeTool_Deactivate();
		return;
	}
	switch (toolId) {
	case TOOL_ERASER:
	case TOOL_PENCIL:
	case TOOL_BRUSH:
	case TOOL_AIRBRUSH:
		FreehandTool_Deactivate();
		break;
	case TOOL_MAGNIFIER:
		MagnifierTool_Deactivate();
		break;
	case TOOL_TEXT:
		TextTool_Deactivate();
		break;
	case TOOL_CURVE:
		BezierTool_Deactivate();
		break;
	case TOOL_POLYGON:
		PolygonTool_Deactivate();
		break;
	case TOOL_PEN:
		PenTool_Deactivate();
		break;
	case TOOL_HIGHLIGHTER:
		HighlighterTool_Deactivate();
		break;
	case TOOL_CRAYON:
		CrayonTool_Deactivate();
		break;
	default:
		break;
	}
}
static void Tool_KillAirbrushTimer(void) {
	HWND hc = GetCanvasWindow();
	if (hc)
		KillTimer(hc, TIMER_AIRBRUSH);
}
static void Tool_HandleCaptureLostForTool(int toolId) {
	if (!Tool_IsValidId(toolId))
		return;
	if (toolId == TOOL_AIRBRUSH)
		Tool_KillAirbrushTimer();
	if (Tool_IsSelectionTool(toolId)) {
		SelectionTool_OnCaptureLost();
		return;
	}
	if (toolId == TOOL_MAGNIFIER) {
		MagnifierTool_OnCaptureLost();
		return;
	}
	if (Tool_IsStrokeTool(toolId)) {
		if (Interaction_IsActive())
			Interaction_OnCaptureLost(Tool_CaptureLostHistoryLabel(toolId));
		return;
	}
	if (Tool_IsModalTool(toolId)) {
		Tool_CancelTool(toolId, TOOL_CANCEL_INTERRUPT);
		return;
	}
}
void ToolCancel(ToolCancelReason reason, BOOL skipSelectionTools) {
	int toolId = Tool_GetCurrent();
	HWND hc = GetCanvasWindow();
	BOOL hadTrackedCapture = (s_runtime.captureOwner != NULL);
	Tool_ClearPointerRuntime();
	if (skipSelectionTools && SelectionIsDragging() && hc) {
		ToolPointerEvent ev = {TOOL_POINTER_UP, hc, {0, 0}, 0};
		SelectionTool_OnPointer(&ev);
	}
	if (!skipSelectionTools || !Tool_IsSelectionTool(toolId))
		Tool_CancelTool(toolId, reason);
	if (hadTrackedCapture || (hc && GetCapture() == hc))
		ReleaseCapture();
	if (hc)
		InvalidateRect(hc, NULL, FALSE);
}
static void ToolOnCaptureLost(void) {
	if (s_runtime.captureOwner == NULL)
		return;
	int toolId = (s_runtime.pointerActive && Tool_IsValidId(s_runtime.pointerTool)) ? s_runtime.pointerTool : Tool_GetCurrent();
	Tool_ClearPointerRuntime();
	Tool_HandleCaptureLostForTool(toolId);
}
void ResetToolStateForNewDocument(void) {
	ToolCancel(TOOL_CANCEL_ABORT, FALSE);
	ToolSession_ClearAllPending();
}
void ToolHandlePointerEvent(const ToolPointerEvent *ev) {
	if (!ev)
		return;
	int activeTool;
	if (ev->type == TOOL_POINTER_DOWN) {
		activeTool = ResolveActiveToolForMouseDown();
		s_runtime.pointerTool = activeTool;
		s_runtime.pointerActive = TRUE;
		s_runtime.captureOwner = NULL;
	} else if (ev->type == TOOL_POINTER_DOUBLE_CLICK) {
		activeTool = Tool_GetCurrent();
	} else {
		activeTool = ResolveActiveToolForMoveUp();
	}
#ifdef _DEBUG
	assert(Tool_IsValidId(activeTool));
	if ((ev->type == TOOL_POINTER_MOVE || ev->type == TOOL_POINTER_UP) && s_runtime.pointerActive) {
		assert(Tool_IsValidId(s_runtime.pointerTool));
	}
#endif
	if (ev->type == TOOL_POINTER_UP)
		s_runtime.captureOwner = NULL;
	Tool_DispatchPointer(ev, activeTool);
	if (ev->type == TOOL_POINTER_DOWN && ev->hwnd && GetCapture() == ev->hwnd) {
		s_runtime.captureOwner = ev->hwnd;
	} else if (ev->type == TOOL_POINTER_UP) {
		BOOL releaseLeftoverCapture = ev->hwnd && GetCapture() == ev->hwnd;
		Tool_ClearPointerRuntime();
		if (releaseLeftoverCapture)
			ReleaseCapture();
	}
}
static void ToolOnViewportChanged(HWND hWnd) {
	if (Tool_GetCurrent() == TOOL_TEXT)
		TextTool_OnViewportChanged(hWnd);
}
void ToolHandleLifecycleEvent(ToolLifecycleEventType type, HWND hWnd) {
	switch (type) {
	case TOOL_LIFECYCLE_CANCEL:
		ToolCancel(TOOL_CANCEL_ABORT, FALSE);
		break;
	case TOOL_LIFECYCLE_CANCEL_INTERRUPT:
		ToolCancel(TOOL_CANCEL_INTERRUPT, FALSE);
		break;
	case TOOL_LIFECYCLE_CAPTURE_LOST:
		ToolOnCaptureLost();
		break;
	case TOOL_LIFECYCLE_VIEWPORT_CHANGED:
		ToolOnViewportChanged(hWnd);
		break;
	case TOOL_LIFECYCLE_RESET_FOR_NEW_DOCUMENT:
		ResetToolStateForNewDocument();
		break;
	case TOOL_LIFECYCLE_TIMER_TICK:
		if (Interaction_IsActive() && Interaction_GetActiveToolId() == TOOL_AIRBRUSH)
			FreehandTool_OnTimerTick();
		break;
	default:
		break;
	}
}
void InitializeTools(void) {
	s_runtime.currentTool = TOOL_PENCIL;
	s_runtime.pointerTool = -1;
	s_runtime.captureOwner = NULL;
	s_runtime.pointerActive = FALSE;
	srand(GetTickCount());
	CrayonTool_RegisterPresets();
	HighlighterTool_RegisterPresets();
	Preset_LoadAll();
}
void ToolDrawOverlay(const OverlayContext *ctx) {
	if (ctx)
		Tool_DrawOverlayForCurrent(ctx);
}
/* Legacy public name: deactivates the current tool using that tool's existing
   semantics. Some tools commit on deactivate; modal draft tools abort. */
void Tool_FinalizeCurrentState(void) {
	Tool_DeactivateTool(Tool_GetCurrent());
}
void SetCurrentTool(int nTool) {
	if (!Tool_IsValidId(nTool)) {
		Tool_DebugInvalidId(nTool);
		return;
	}
	if (s_runtime.currentTool == nTool)
		return;
	if (s_runtime.pointerActive || s_runtime.captureOwner != NULL)
		ToolCancel(TOOL_CANCEL_ABORT, FALSE);
	Tool_FinalizeCurrentState();
	s_runtime.currentTool = nTool;
	UpdateToolOptions(nTool);
	HWND hToolbar = GetToolbarWindow();
	if (hToolbar)
		InvalidateRect(hToolbar, NULL, FALSE);
}
int Tool_GetCurrent(void) {
	return s_runtime.currentTool;
}
