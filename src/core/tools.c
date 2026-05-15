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
typedef void (*CancelFn)(ToolCancelReason);
typedef BOOL (*BusyFn)(void);

typedef struct {
  PointerFn down, move, up, dblclick;
  OverlayFn overlay;
  VoidFn    deactivate;
  CancelFn  cancel;
  BusyFn    isBusy;
  const char *captureLostLabel;
} ToolVTable;

/* Adapters: forward to impls that need extra args, or signature-bridge cancel. */
static void FH_Down(HWND h,int x,int y,int b){ FreehandTool_OnMouseDown(h,x,y,b,Tool_GetCurrent()); }
static void FH_Move(HWND h,int x,int y,int b){ FreehandTool_OnMouseMove(h,x,y,b,Tool_GetCurrent()); }
static void FH_Up  (HWND h,int x,int y,int b){ FreehandTool_OnMouseUp  (h,x,y,b,Tool_GetCurrent()); }

static void SH_Down(HWND h,int x,int y,int b){ ShapeTool_OnMouseDown(h,x,y,b,Tool_GetCurrent()); }
static void SH_Move(HWND h,int x,int y,int b){ ShapeTool_OnMouseMove(h,x,y,b,Tool_GetCurrent()); }
static void SH_Up  (HWND h,int x,int y,int b){ ShapeTool_OnMouseUp  (h,x,y,b,Tool_GetCurrent()); }

static void C_Freehand   (ToolCancelReason r){(void)r; FreehandTool_Cancel();}
static void C_Shape      (ToolCancelReason r){(void)r; ShapeTool_Cancel();}
static void C_Bezier     (ToolCancelReason r){(void)r; BezierTool_Cancel();}
static void C_Polygon    (ToolCancelReason r){(void)r; PolygonTool_Cancel();}
static void C_Pen        (ToolCancelReason r){(void)r; PenTool_Cancel();}
static void C_Highlighter(ToolCancelReason r){(void)r; HighlighterTool_Cancel();}
static void C_Crayon     (ToolCancelReason r){(void)r; CrayonTool_Cancel();}
static void C_Magnifier  (ToolCancelReason r){(void)r; MagnifierTool_Deactivate();}
static void C_Text       (ToolCancelReason r){(void)r; TextTool_Cancel();}
static void C_Selection  (ToolCancelReason r){ SelectionTool_Cancel(r); }

#define VT_SELECTION   { SelectionTool_OnMouseDown, SelectionTool_OnMouseMove, SelectionTool_OnMouseUp, NULL, SelectionTool_DrawOverlay, SelectionTool_Deactivate, C_Selection, NULL, NULL }
#define VT_FREEHAND    { FH_Down, FH_Move, FH_Up, NULL, NULL, FreehandTool_Deactivate, C_Freehand, IsFreehandDrawing, "Draw" }
#define VT_SHAPE       { SH_Down, SH_Move, SH_Up, NULL, ShapeTool_DrawOverlay, ShapeTool_Deactivate, C_Shape, ShapeTool_IsBusy, "Draw Shape" }

static const ToolVTable s_tools[TOOL_CRAYON + 1] = {
  [TOOL_FREEFORM]   = VT_SELECTION,
  [TOOL_SELECT]     = VT_SELECTION,
  [TOOL_ERASER]     = VT_FREEHAND,
  [TOOL_PENCIL]     = VT_FREEHAND,
  [TOOL_BRUSH]      = VT_FREEHAND,
  [TOOL_AIRBRUSH]   = { AirbrushTool_OnMouseDown, AirbrushTool_OnMouseMove, AirbrushTool_OnMouseUp, NULL, NULL, FreehandTool_Deactivate, C_Freehand, IsFreehandDrawing, "Draw" },
  [TOOL_FILL]       = { FillTool_OnMouseDown, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  [TOOL_PICK]       = { PickTool_OnMouseDown, PickTool_OnMouseMove, NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  [TOOL_MAGNIFIER]  = { MagnifierTool_OnMouseDown, MagnifierTool_OnMouseMove, MagnifierTool_OnMouseUp, NULL, MagnifierTool_DrawOverlay, MagnifierTool_Deactivate, C_Magnifier, NULL, NULL },
  [TOOL_TEXT]       = { TextTool_OnMouseDown, TextTool_OnMouseMove, TextTool_OnMouseUp, NULL, TextTool_DrawOverlay, TextTool_Deactivate, C_Text, IsTextEditing, NULL },
  [TOOL_LINE]       = VT_SHAPE,
  [TOOL_RECT]       = VT_SHAPE,
  [TOOL_ELLIPSE]    = VT_SHAPE,
  [TOOL_ROUNDRECT]  = VT_SHAPE,
  [TOOL_CURVE]      = { BezierTool_OnMouseDown, BezierTool_OnMouseMove, BezierTool_OnMouseUp, NULL, BezierTool_DrawOverlay, BezierTool_Deactivate, C_Bezier, IsCurvePending, "Draw Curve" },
  [TOOL_POLYGON]    = { PolygonTool_OnMouseDown, PolygonTool_OnMouseMove, PolygonTool_OnMouseUp, PolygonTool_OnDoubleClick, PolygonTool_DrawOverlay, PolygonTool_Deactivate, C_Polygon, IsPolygonPending, "Draw Polygon" },
  [TOOL_PEN]        = { PenTool_OnMouseDown, PenTool_OnMouseMove, PenTool_OnMouseUp, NULL, NULL, PenTool_Deactivate, C_Pen, IsPenDrawing, NULL },
  [TOOL_HIGHLIGHTER]= { HighlighterTool_OnMouseDown, HighlighterTool_OnMouseMove, HighlighterTool_OnMouseUp, NULL, NULL, HighlighterTool_Deactivate, C_Highlighter, IsHighlighterDrawing, NULL },
  [TOOL_CRAYON]     = { CrayonTool_OnMouseDown, CrayonTool_OnMouseMove, CrayonTool_OnMouseUp, NULL, NULL, CrayonTool_Deactivate, C_Crayon, IsCrayonDrawing, NULL },
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
  const ToolVTable *vt = VT(toolId);
  if (vt && vt->cancel) vt->cancel(reason);
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

void CommitCurrentSelection(void) {
  if (IsSelectionActive()) CommitSelection();
}

void ClearSelection(void) { SelectionDelete(); }

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
