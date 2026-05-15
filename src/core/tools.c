#include "peztold_core.h"
#include "canvas.h"
#include "controller.h"
#include "app_commands.h"
#include "gdi_utils.h"
#include "geom.h"
#include "history.h"
#include "layers.h"
#include "tools.h"
#include "tool_session.h"

#include "tools/bezier_tool.h"
#include "tools/crayon_tool.h"
#include "tools/magnifier_tool.h"
#include "tools/fill_tool.h"
#include "tools/freehand_tools.h"
#include "tools/highlighter_tool.h"
#include "tools/pen_tool.h"
#include "tools/pick_tool.h"
#include "tools/polygon_tool.h"
#include "tools/selection_tool.h"
#include "tools/shape_tools.h"
#include "interaction.h"
#include "tools/text_tool.h"
#include "tools/tool_options/presets.h"
#include "tools/tool_options/tool_options.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/toolbar.h"
#include <stdlib.h>

int currentTool = TOOL_PENCIL;

static struct {
  int activeToolAtMouseDown;
  HWND capturedWindow;
} s_runtime = {-1, NULL};

typedef void (*PointerFn)(HWND, int, int, int);
typedef void (*OverlayFn)(HDC, double, int, int);
typedef void (*VoidFn)(void);
typedef BOOL (*BusyFn)(void);

typedef struct {
  PointerFn down, move, up, dblclick;
  OverlayFn overlay;
  VoidFn    deactivate;
  VoidFn    cancel;
  BusyFn    isBusy;
  const char *captureLostLabel;
} ToolVTable;

#define VT_SELECTION   { SelectionTool_OnMouseDown, SelectionTool_OnMouseMove, SelectionTool_OnMouseUp, NULL, SelectionTool_DrawOverlay, SelectionTool_Deactivate, NULL, NULL, NULL }
#define VT_FREEHAND    { FreehandTool_OnMouseDown, FreehandTool_OnMouseMove, FreehandTool_OnMouseUp, NULL, NULL, FreehandTool_Deactivate, (VoidFn)FreehandTool_Cancel, IsFreehandDrawing, "Draw" }
#define VT_SHAPE       { ShapeTool_OnMouseDown, ShapeTool_OnMouseMove, ShapeTool_OnMouseUp, NULL, ShapeTool_DrawOverlay, ShapeTool_Deactivate, (VoidFn)ShapeTool_Cancel, IsShapePending, "Draw Shape" }

static const ToolVTable s_tools[TOOL_CRAYON + 1] = {
  [TOOL_FREEFORM]   = VT_SELECTION,
  [TOOL_SELECT]     = VT_SELECTION,
  [TOOL_ERASER]     = VT_FREEHAND,
  [TOOL_PENCIL]     = VT_FREEHAND,
  [TOOL_BRUSH]      = VT_FREEHAND,
  [TOOL_AIRBRUSH]   = { AirbrushTool_OnMouseDown, AirbrushTool_OnMouseMove, AirbrushTool_OnMouseUp, NULL, NULL, FreehandTool_Deactivate, (VoidFn)FreehandTool_Cancel, IsFreehandDrawing, "Draw" },
  [TOOL_FILL]       = { FillTool_OnMouseDown, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  [TOOL_PICK]       = { PickTool_OnMouseDown, PickTool_OnMouseMove, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  [TOOL_MAGNIFIER]  = { MagnifierTool_OnMouseDown, MagnifierTool_OnMouseMove, MagnifierTool_OnMouseUp, NULL, MagnifierTool_DrawOverlay, MagnifierTool_Deactivate, (VoidFn)MagnifierTool_Deactivate, NULL, NULL },
  [TOOL_TEXT]       = { TextTool_OnMouseDown, TextTool_OnMouseMove, TextTool_OnMouseUp, NULL, TextTool_DrawOverlay, TextTool_Deactivate, (VoidFn)TextTool_Cancel, IsTextEditing, NULL },
  [TOOL_LINE]       = VT_SHAPE,
  [TOOL_RECT]       = VT_SHAPE,
  [TOOL_ELLIPSE]    = VT_SHAPE,
  [TOOL_ROUNDRECT]  = VT_SHAPE,
  [TOOL_CURVE]      = { BezierTool_OnMouseDown, BezierTool_OnMouseMove, BezierTool_OnMouseUp, NULL, BezierTool_DrawOverlay, BezierTool_Deactivate, (VoidFn)BezierTool_Cancel, IsCurvePending, "Draw Curve" },
  [TOOL_POLYGON]    = { PolygonTool_OnMouseDown, PolygonTool_OnMouseMove, PolygonTool_OnMouseUp, PolygonTool_OnDoubleClick, PolygonTool_DrawOverlay, PolygonTool_Deactivate, (VoidFn)PolygonTool_Cancel, IsPolygonPending, "Draw Polygon" },
  [TOOL_PEN]        = { PenTool_OnMouseDown, PenTool_OnMouseMove, PenTool_OnMouseUp, NULL, NULL, PenTool_Deactivate, (VoidFn)PenTool_Cancel, IsPenDrawing, NULL },
  [TOOL_HIGHLIGHTER]= { HighlighterTool_OnMouseDown, HighlighterTool_OnMouseMove, HighlighterTool_OnMouseUp, NULL, NULL, HighlighterTool_Deactivate, (VoidFn)HighlighterTool_Cancel, IsHighlighterDrawing, NULL },
  [TOOL_CRAYON]     = { CrayonTool_OnMouseDown, CrayonTool_OnMouseMove, CrayonTool_OnMouseUp, NULL, NULL, CrayonTool_Deactivate, (VoidFn)CrayonTool_Cancel, IsCrayonDrawing, NULL },
};

static const ToolVTable *VT(int toolId) {
  if (toolId < 0 || toolId >= (int)(sizeof(s_tools)/sizeof(s_tools[0]))) return NULL;
  return &s_tools[toolId];
}

static BOOL ToolIsBusy(int toolId) {
  const ToolVTable *vt = VT(toolId);
  return (vt && vt->isBusy) ? vt->isBusy() : FALSE;
}

BOOL Tool_IsCurrentBusy(void) { return ToolIsBusy(Tool_GetCurrent()); }

static int ResolveActiveToolForMouseDown(void) {
  return (IsAltDown() && GetCapture() == NULL) ? TOOL_PICK : Tool_GetCurrent();
}

static int ResolveActiveToolForMoveUp(HWND hWnd) {
  int resolved = (hWnd && GetCapture() == hWnd && s_runtime.activeToolAtMouseDown >= 0)
                     ? s_runtime.activeToolAtMouseDown
                     : (IsAltDown() ? TOOL_PICK : Tool_GetCurrent());
  if (resolved == TOOL_PICK && IsAltDown() && ToolIsBusy(Tool_GetCurrent()))
    return Tool_GetCurrent();
  return resolved;
}

static void ToolCancelInternal(int toolId, ToolCancelReason reason) {
  if (toolId == TOOL_SELECT || toolId == TOOL_FREEFORM) {
    SelectionTool_Cancel(reason);
    return;
  }
  const ToolVTable *vt = VT(toolId);
  if (vt && vt->cancel) vt->cancel();
}

void ToolCancel(ToolCancelReason reason, BOOL skipSelectionTools) {
  int t = Tool_GetCurrent();
  HWND hc = GetCanvasWindow();
  HWND hCap = s_runtime.capturedWindow;
  s_runtime.activeToolAtMouseDown = -1;
  s_runtime.capturedWindow = NULL;

  if (skipSelectionTools && SelectionIsDragging() && hc)
    SelectionTool_OnMouseUp(hc, 0, 0, 0);

  if (!skipSelectionTools || (t != TOOL_FREEFORM && t != TOOL_SELECT))
    ToolCancelInternal(t, reason);

  if (hCap || (hc && GetCapture() == hc))
    ReleaseCapture();

  if (hc)
    InvalidateRect(hc, NULL, FALSE);
}

static void ToolOnCaptureLost(void) {
  if (s_runtime.capturedWindow == NULL)
    return;

  if (Interaction_IsActive()) {
    const ToolVTable *vt = VT(Interaction_GetActiveToolId());
    const char *label = (vt && vt->captureLostLabel) ? vt->captureLostLabel : "Draw";
    Interaction_OnCaptureLost(label);
  }

  HWND hc = GetCanvasWindow();
  if (hc && Tool_GetCurrent() == TOOL_AIRBRUSH)
    KillTimer(hc, TIMER_AIRBRUSH);

  ToolCancel(TOOL_CANCEL_INTERRUPT, FALSE);
}

void ResetToolStateForNewDocument(void) {
  ToolCancel(TOOL_CANCEL_ABORT, FALSE);
  ToolSession_ClearAllPending();
}

static void DispatchPointer(HWND hWnd, int x, int y, int btn, int toolId, ToolPointerEventType type) {
  const ToolVTable *vt = VT(toolId);
  if (!vt) return;
  PointerFn fn = NULL;
  switch (type) {
  case TOOL_POINTER_DOWN:         fn = vt->down; break;
  case TOOL_POINTER_MOVE:         fn = vt->move; break;
  case TOOL_POINTER_UP:           fn = vt->up; break;
  case TOOL_POINTER_DOUBLE_CLICK: fn = vt->dblclick; break;
  default: return;
  }
  if (fn) fn(hWnd, x, y, btn);
}

void ToolHandlePointerEvent(ToolPointerEventType type, HWND hWnd, int x, int y, int nButton) {
  int activeTool;
  if (type == TOOL_POINTER_DOWN) {
    activeTool = ResolveActiveToolForMouseDown();
    s_runtime.activeToolAtMouseDown = activeTool;
    s_runtime.capturedWindow = NULL;
  } else if (type == TOOL_POINTER_DOUBLE_CLICK) {
    activeTool = Tool_GetCurrent();
  } else {
    activeTool = ResolveActiveToolForMoveUp(hWnd);
  }

  DispatchPointer(hWnd, x, y, nButton, activeTool, type);

  if (type == TOOL_POINTER_DOWN && GetCapture() == hWnd) {
    s_runtime.capturedWindow = hWnd;
  } else if (type == TOOL_POINTER_UP) {
    s_runtime.activeToolAtMouseDown = -1;
    s_runtime.capturedWindow = NULL;
  }
}

static void ToolOnViewportChanged(HWND hWnd) {
  if (Tool_GetCurrent() == TOOL_TEXT)
    TextTool_OnViewportChanged(hWnd);
}

void ToolHandleLifecycleEvent(ToolLifecycleEventType type, HWND hWnd) {
  switch (type) {
  case TOOL_LIFECYCLE_CANCEL:           ToolCancel(TOOL_CANCEL_ABORT, FALSE); break;
  case TOOL_LIFECYCLE_CANCEL_INTERRUPT: ToolCancel(TOOL_CANCEL_INTERRUPT, FALSE); break;
  case TOOL_LIFECYCLE_CAPTURE_LOST:     ToolOnCaptureLost(); break;
  case TOOL_LIFECYCLE_VIEWPORT_CHANGED: ToolOnViewportChanged(hWnd); break;
  case TOOL_LIFECYCLE_RESET_FOR_NEW_DOCUMENT: ResetToolStateForNewDocument(); break;
  case TOOL_LIFECYCLE_TIMER_TICK:
    if (Tool_GetCurrent() == TOOL_AIRBRUSH) FreehandTool_OnTimerTick();
    break;
  default: break;
  }
}

void InitializeTools(void) {
  currentTool = TOOL_PENCIL;
  s_runtime.activeToolAtMouseDown = -1;
  s_runtime.capturedWindow = NULL;
  srand(GetTickCount());
  CrayonTool_RegisterPresets();
  HighlighterTool_RegisterPresets();
  Preset_LoadAll();
}

void ToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  const ToolVTable *vt = VT(Tool_GetCurrent());
  if (vt && vt->overlay) vt->overlay(hdc, dScale, nDestX, nDestY);
}

void Tool_FinalizeCurrentState(void) {
  const ToolVTable *vt = VT(Tool_GetCurrent());
  if (vt && vt->deactivate) vt->deactivate();
}

void SetCurrentTool(int nTool) {
  if (currentTool == nTool) return;

  if (s_runtime.capturedWindow != NULL)
    ToolCancel(TOOL_CANCEL_ABORT, FALSE);

  Tool_FinalizeCurrentState();

  currentTool = nTool;

  UpdateToolOptions(nTool);
  HWND hToolbar = GetToolbarWindow();
  if (hToolbar) InvalidateRect(hToolbar, NULL, FALSE);
}

int Tool_GetCurrent(void) { return currentTool; }
