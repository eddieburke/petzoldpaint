/*------------------------------------------------------------
   TOOLS.C -- Tool System and VTable Registry

   This module provides the tool dispatch system, routing mouse
   events and drawing calls to the appropriate tool implementations.
  ------------------------------------------------------------*/

#include "peztold_core.h"
#include "canvas.h"
#include "app_commands.h"
#include "gdi_utils.h"
#include "geom.h"
#include "history.h"
#include "layers.h"
#include "tools.h"

// Tool implementations
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
#include "interaction.h"
#include "tools/text_tool.h"
#include "tools/tool_options/presets.h"
#include "tools/tool_options/tool_options.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/toolbar.h"
#include <stdlib.h>

/*------------------------------------------------------------
   Global Tool State Variables
  ------------------------------------------------------------*/

int currentTool = TOOL_PENCIL;

/*------------------------------------------------------------
   Keypress State Management - Prevents conflicts when
   pressing modifier keys during active mouse operations
  ------------------------------------------------------------*/

static struct { int activeToolAtMouseDown; HWND capturedWindow; } s_runtime = {-1, NULL};

/*------------------------------------------------------------
   Tool Resolution Functions with Keypress State Management
  ------------------------------------------------------------*/

static BOOL ToolIsBusy(int toolId) {
  switch (toolId) {
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH:
    case TOOL_AIRBRUSH: return IsFreehandDrawing();
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: return ShapeTool_IsBusy();
    case TOOL_CURVE: return IsCurvePending();
    case TOOL_POLYGON: return IsPolygonPending();
    case TOOL_PEN: return IsPenDrawing();
    case TOOL_HIGHLIGHTER: return IsHighlighterDrawing();
    case TOOL_CRAYON: return IsCrayonDrawing();
    default: return FALSE;
  }
}

// Resolve tool for mouse down - allows temporary tool switch
static int ResolveActiveToolForMouseDown(void) {
  return ((IsAltDown() && GetCapture() == NULL) ? TOOL_PICK : Tool_GetCurrent());
}

// Resolve tool for mouse move/up - respects captured state
static int ResolveActiveToolForMoveUp(HWND hWnd) {
  int resolved = ((hWnd && GetCapture()==hWnd && s_runtime.activeToolAtMouseDown>=0) ? s_runtime.activeToolAtMouseDown : (IsAltDown() ? TOOL_PICK : Tool_GetCurrent()));
  if (resolved == TOOL_PICK && IsAltDown()) {
    if (ToolIsBusy(Tool_GetCurrent())) {
      return Tool_GetCurrent();
    }
  }
  return resolved;
}

static void ToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
static void ToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
static void ToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
static void ToolOnDoubleClick(HWND hWnd, int x, int y, int nButton);
static void ToolOnCaptureLost(void);
static void ToolOnViewportChanged(HWND hWnd);

/*------------------------------------------------------------
   Modal Tool Commit/Cancel System
  ------------------------------------------------------------*/

static void ToolCancelInternal(int toolId, ToolCancelReason reason) {
  switch (toolId) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionTool_Cancel(reason); break;
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH:
    case TOOL_AIRBRUSH: CancelFreehandDrawing(); break;
    case TOOL_TEXT: CancelText(); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeTool_Cancel(); break;
    case TOOL_CURVE: BezierTool_Cancel(); break;
    case TOOL_POLYGON: PolygonTool_Cancel(); break;
    case TOOL_PEN: CancelPenDrawing(); break;
    case TOOL_HIGHLIGHTER: CancelHighlighterDrawing(); break;
    case TOOL_CRAYON: CancelCrayonDrawing(); break;
  }
}

void ToolCancel(ToolCancelReason reason) {
  int t = Tool_GetCurrent();
  HWND hc = GetCanvasWindow();
  HWND hCap = s_runtime.capturedWindow;
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  ToolCancelInternal(t, reason);

  if (hCap || (hc && GetCapture() == hc)) {
    ReleaseCapture();
  }

  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static void ToolOnCaptureLost(void) {
  // If hCapturedWindow is NULL, it means we voluntarily released capture in ToolOnMouseUp.
  // In this case, we don't want to cancel the tool's state (e.g. selection/text box).
  if (s_runtime.capturedWindow == NULL) {
    return;
  }

  if (Interaction_IsActive()) {
    int activeStrokeTool = Tool_GetCurrent();
    const char *action = (activeStrokeTool == TOOL_LINE || activeStrokeTool == TOOL_RECT ||
                          activeStrokeTool == TOOL_ELLIPSE || activeStrokeTool == TOOL_ROUNDRECT)
                             ? "Draw Shape" : "Draw";
    Interaction_OnCaptureLost(action);
  }

  ToolCancel(TOOL_CANCEL_INTERRUPT); /* abort any in-progress tool that never got MouseUp */
}

void ResetToolStateForNewDocument(void) {
  // Cancel any modal tool state and clear transient interactions
  ToolCancel(TOOL_CANCEL_ABORT);
}

void ToolHandlePointerEvent(ToolPointerEventType type, HWND hWnd, int x, int y,
                            int nButton) {
  switch (type) {
  case TOOL_POINTER_DOWN:
    ToolOnMouseDown(hWnd, x, y, nButton);
    break;
  case TOOL_POINTER_MOVE:
    ToolOnMouseMove(hWnd, x, y, nButton);
    break;
  case TOOL_POINTER_UP:
    ToolOnMouseUp(hWnd, x, y, nButton);
    break;
  case TOOL_POINTER_DOUBLE_CLICK:
    ToolOnDoubleClick(hWnd, x, y, nButton);
    break;
  default:
    break;
  }
}

void ToolHandleLifecycleEvent(ToolLifecycleEventType type, HWND hWnd) {
  switch (type) {
  case TOOL_LIFECYCLE_CANCEL:
    ToolCancel(TOOL_CANCEL_ABORT);
    break;
  case TOOL_LIFECYCLE_CANCEL_INTERRUPT:
    ToolCancel(TOOL_CANCEL_INTERRUPT);
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
    if (Tool_GetCurrent() == TOOL_AIRBRUSH) {
      FreehandTool_OnTimerTick();
    }
    break;
  default:
    break;
  }
}

/*------------------------------------------------------------
   Public Tool API Functions
  ------------------------------------------------------------*/

void InitializeTools(void) {
  currentTool = TOOL_PENCIL;
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;
  srand(GetTickCount());
  CrayonTool_RegisterPresets();
  HighlighterTool_RegisterPresets();
  Preset_LoadAll();
}

void CommitCurrentSelection(void) {
  if (IsSelectionActive())
    CommitSelection();
}

void ClearSelection(void) { SelectionDelete(); }

static void ToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMouseDown();

  // Store the active tool and window at mouse down to prevent state conflicts
  // This ensures that if the user presses Alt during a drag, we don't switch
  // to the eyedropper mid-operation
  s_runtime.activeToolAtMouseDown = activeTool; s_runtime.capturedWindow = NULL;

  switch (activeTool) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH: FreehandTool_OnMouseDown(hWnd, x, y, nButton, activeTool); break;
    case TOOL_AIRBRUSH: AirbrushToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_FILL: FillToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_PICK: PickToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_MAGNIFIER: MagnifierToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_TEXT: TextToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeTool_OnMouseDown(hWnd, x, y, nButton, activeTool); break;
    case TOOL_CURVE: BezierToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_POLYGON: PolygonTool_OnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_PEN: PenToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_HIGHLIGHTER: HighlighterToolOnMouseDown(hWnd, x, y, nButton); break;
    case TOOL_CRAYON: CrayonToolOnMouseDown(hWnd, x, y, nButton); break;
  }

  // Track if this tool captured the mouse
  if (GetCapture() == hWnd) s_runtime.capturedWindow = hWnd;
}

static void ToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);
  switch (activeTool) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH: FreehandTool_OnMouseMove(hWnd, x, y, nButton, activeTool); break;
    case TOOL_AIRBRUSH: AirbrushToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_PICK: PickToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_MAGNIFIER: MagnifierToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_TEXT: TextToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeTool_OnMouseMove(hWnd, x, y, nButton, activeTool); break;
    case TOOL_CURVE: BezierToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_POLYGON: PolygonTool_OnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_PEN: PenToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_HIGHLIGHTER: HighlighterToolOnMouseMove(hWnd, x, y, nButton); break;
    case TOOL_CRAYON: CrayonToolOnMouseMove(hWnd, x, y, nButton); break;
  }
}

static void ToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);

  // Always clear active-tool-at-mouse-down on button up so modifier keys
  // work correctly for the next operation. Relying on GetCapture() can leave
  // state stale if a tool does not capture or capture is lost.
  // Clearing BEFORE dispatching prevents voluntary ReleaseCapture() inside
  // tools from triggering an abort via WM_CAPTURECHANGED.
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  switch (activeTool) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH: FreehandTool_OnMouseUp(hWnd, x, y, nButton, activeTool); break;
    case TOOL_AIRBRUSH: AirbrushToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_MAGNIFIER: MagnifierToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_TEXT: TextToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeTool_OnMouseUp(hWnd, x, y, nButton, activeTool); break;
    case TOOL_CURVE: BezierToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_POLYGON: PolygonTool_OnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_PEN: PenToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_HIGHLIGHTER: HighlighterToolOnMouseUp(hWnd, x, y, nButton); break;
    case TOOL_CRAYON: CrayonToolOnMouseUp(hWnd, x, y, nButton); break;
  }
}

static void ToolOnDoubleClick(HWND hWnd, int x, int y, int nButton) {
  if (Tool_GetCurrent() == TOOL_POLYGON) {
    PolygonTool_OnDoubleClick(hWnd, x, y, nButton);
  }
}

void ToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  switch (Tool_GetCurrent()) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionToolDrawOverlay(hdc, dScale, nDestX, nDestY); break;
    case TOOL_MAGNIFIER: MagnifierToolDrawOverlay(hdc, dScale, nDestX, nDestY); break;
    case TOOL_TEXT: TextToolDrawOverlay(hdc, dScale, nDestX, nDestY); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeToolDrawOverlay(hdc, dScale, nDestX, nDestY); break;
    case TOOL_CURVE: BezierToolDrawOverlay(hdc, dScale, nDestX, nDestY); break;
    case TOOL_POLYGON: PolygonTool_DrawOverlay(hdc, dScale, nDestX, nDestY); break;
  }
}

void Tool_FinalizeCurrentState(void) {
  switch (Tool_GetCurrent()) {
    case TOOL_FREEFORM:
    case TOOL_SELECT: SelectionTool_Deactivate(); break;
    case TOOL_ERASER:
    case TOOL_PENCIL:
    case TOOL_BRUSH:
    case TOOL_AIRBRUSH: FreehandTool_Deactivate(); break;
    case TOOL_MAGNIFIER: MagnifierToolDeactivate(); break;
    case TOOL_TEXT: TextTool_Deactivate(); break;
    case TOOL_LINE:
    case TOOL_RECT:
    case TOOL_ELLIPSE:
    case TOOL_ROUNDRECT: ShapeTool_Deactivate(); break;
    case TOOL_CURVE: BezierTool_Deactivate(); break;
    case TOOL_POLYGON: PolygonTool_Deactivate(); break;
    case TOOL_PEN: PenTool_Deactivate(); break;
    case TOOL_HIGHLIGHTER: HighlighterTool_Deactivate(); break;
    case TOOL_CRAYON: CrayonTool_Deactivate(); break;
  }
}

void SetCurrentTool(int nTool) {
  if (currentTool == nTool)
    return;

  if (s_runtime.capturedWindow != NULL) {
    ToolCancel(TOOL_CANCEL_ABORT);
  }

  /* Finalize the outgoing tool */
  Tool_FinalizeCurrentState();

  currentTool = nTool;

  /* Activate the incoming tool */
  // Currently no tools use onActivate in PetzoldPaint.

  UpdateToolOptions(nTool);
  HWND hToolbar = GetToolbarWindow();
  if (hToolbar)
    InvalidateRect(hToolbar, NULL, FALSE);
}

static void ToolOnViewportChanged(HWND hWnd) {
  if (Tool_GetCurrent() == TOOL_TEXT) {
    TextToolOnViewportChanged(hWnd);
  }
}

int Tool_GetCurrent(void) { return currentTool; }
