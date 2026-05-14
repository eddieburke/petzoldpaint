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
#include "tools/stroke_session.h"
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
   Tool VTable Type Definitions
  ------------------------------------------------------------*/

typedef void (*ToolMouseDownFn)(HWND, int, int, int);
typedef void (*ToolMouseMoveFn)(HWND, int, int, int);
typedef void (*ToolMouseUpFn)(HWND, int, int, int);
typedef void (*ToolDblClickFn)(HWND, int, int, int);
typedef void (*ToolDrawOverlayFn)(HDC, double, int, int);
typedef void (*ToolActivateFn)(void);
typedef void (*ToolDeactivateFn)(void);
typedef BOOL (*ToolCancelFn)(void);
typedef void (*ToolViewportChangedFn)(HWND);

typedef struct {
  ToolMouseDownFn onMouseDown;
  ToolMouseMoveFn onMouseMove;
  ToolMouseUpFn onMouseUp;
  ToolDblClickFn onDoubleClick;
  ToolDrawOverlayFn drawOverlay;
  /* Lifecycle methods */
  ToolActivateFn onActivate;
  ToolDeactivateFn onDeactivate;
  ToolCancelFn onCancel;
  ToolViewportChangedFn onViewportChanged;
  BOOL (*isBusy)(void);
  void (*onCaptureLost)(void);
  void (*onTimerTick)(void);
} ToolVTable;

#define TOOL_LIFECYCLE_FREEHAND_COMMON                                           \
  .onActivate = NULL, .onDeactivate = FreehandTool_Deactivate,                  \
  .onCancel = CancelFreehandDrawing, .onViewportChanged = NULL,                 \
  .isBusy = IsFreehandDrawing, .onCaptureLost = NULL

#define TOOL_LIFECYCLE_SHAPE_COMMON                                              \
  .onActivate = NULL, .onDeactivate = ShapeTool_Deactivate,                      \
  .onCancel = ShapeTool_Cancel, .onViewportChanged = NULL,                       \
  .isBusy = ShapeTool_IsBusy, .onCaptureLost = NULL

#define TOOL_LIFECYCLE_MODAL_COMMON(_deactivate, _cancel, _is_busy,              \
                                    _capture_lost)                               \
  .onActivate = NULL, .onDeactivate = (_deactivate), .onCancel = (_cancel),      \
  .onViewportChanged = NULL, .isBusy = (_is_busy), .onCaptureLost = (_capture_lost)

static void SharedFreehandOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  FreehandTool_OnMouseDown(hWnd, x, y, nButton, Tool_GetCurrent());
}

static void SharedFreehandOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  FreehandTool_OnMouseMove(hWnd, x, y, nButton, Tool_GetCurrent());
}

static void SharedFreehandOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  FreehandTool_OnMouseUp(hWnd, x, y, nButton, Tool_GetCurrent());
}

static void SharedShapeOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  ShapeTool_OnMouseDown(hWnd, x, y, nButton, Tool_GetCurrent());
}

static void SharedShapeOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  ShapeTool_OnMouseMove(hWnd, x, y, nButton, Tool_GetCurrent());
}

static void SharedShapeOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  ShapeTool_OnMouseUp(hWnd, x, y, nButton, Tool_GetCurrent());
}

static const ToolVTable *GetToolVTable(int toolId);

/*------------------------------------------------------------
   Tool Resolution Functions with Keypress State Management
  ------------------------------------------------------------*/

// Resolve tool for mouse down - allows temporary tool switch
static int ResolveActiveToolForMouseDown(void) {
  return ((IsAltDown() && GetCapture() == NULL) ? TOOL_PICK : Tool_GetCurrent());
}

// Resolve tool for mouse move/up - respects captured state
static int ResolveActiveToolForMoveUp(HWND hWnd) {
  int resolved = ((hWnd && GetCapture()==hWnd && s_runtime.activeToolAtMouseDown>=0) ? s_runtime.activeToolAtMouseDown : (IsAltDown() ? TOOL_PICK : Tool_GetCurrent()));
  if (resolved == TOOL_PICK && IsAltDown()) {
    const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
    if (tool && tool->isBusy && tool->isBusy()) {
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
   Tool VTable Registry
  ------------------------------------------------------------*/

static const ToolVTable s_ToolTable[] = {
    /* {onMouseDown, onMouseMove, onMouseUp, onDoubleClick,
         drawOverlay, onActivate, onDeactivate, onCancel} */
    [TOOL_FREEFORM] = {.onMouseDown = SelectionToolOnMouseDown,
                       .onMouseMove = SelectionToolOnMouseMove,
                       .onMouseUp = SelectionToolOnMouseUp,
                       .drawOverlay = SelectionToolDrawOverlay,
                       .onDeactivate = SelectionTool_Deactivate,
                       .onCancel = SelectionTool_Cancel,
                       .onCaptureLost = SelectionTool_OnCaptureLost},
    [TOOL_SELECT] = {.onMouseDown = SelectionToolOnMouseDown,
                      .onMouseMove = SelectionToolOnMouseMove,
                      .onMouseUp = SelectionToolOnMouseUp,
                      .drawOverlay = SelectionToolDrawOverlay,
                      .onDeactivate = SelectionTool_Deactivate,
                      .onCancel = SelectionTool_Cancel,
                      .onCaptureLost = SelectionTool_OnCaptureLost},

    [TOOL_ERASER] = {.onMouseDown = SharedFreehandOnMouseDown,
                     .onMouseMove = SharedFreehandOnMouseMove,
                     .onMouseUp = SharedFreehandOnMouseUp,
                     TOOL_LIFECYCLE_FREEHAND_COMMON},
    [TOOL_FILL] = {FillToolOnMouseDown, NULL, NULL, NULL, NULL, NULL,
                    NULL},
    [TOOL_PICK] = {PickToolOnMouseDown, PickToolOnMouseMove, NULL, NULL, NULL,
                    NULL, NULL, NULL},
    [TOOL_MAGNIFIER] = {MagnifierToolOnMouseDown, MagnifierToolOnMouseMove,
                        MagnifierToolOnMouseUp, NULL,
                        MagnifierToolDrawOverlay, NULL, MagnifierToolDeactivate,
                        NULL, NULL, NULL, MagnifierToolDeactivate},
    [TOOL_PENCIL] = {.onMouseDown = SharedFreehandOnMouseDown,
                     .onMouseMove = SharedFreehandOnMouseMove,
                     .onMouseUp = SharedFreehandOnMouseUp,
                     TOOL_LIFECYCLE_FREEHAND_COMMON},
    [TOOL_BRUSH] = {.onMouseDown = SharedFreehandOnMouseDown,
                    .onMouseMove = SharedFreehandOnMouseMove,
                    .onMouseUp = SharedFreehandOnMouseUp,
                    TOOL_LIFECYCLE_FREEHAND_COMMON},
    [TOOL_AIRBRUSH] = {.onMouseDown = AirbrushToolOnMouseDown,
                       .onMouseMove = AirbrushToolOnMouseMove,
                       .onMouseUp = AirbrushToolOnMouseUp,
                       TOOL_LIFECYCLE_FREEHAND_COMMON,
                       .onTimerTick = FreehandTool_OnTimerTick},
    [TOOL_TEXT] = {TextToolOnMouseDown, TextToolOnMouseMove, TextToolOnMouseUp,
                   NULL, TextToolDrawOverlay, NULL,
                   TextTool_Deactivate, CancelText, TextToolOnViewportChanged},

    /* Shape tools: shared controller + policy dispatch with draft-layer rendering. */
    [TOOL_LINE] = {.onMouseDown = SharedShapeOnMouseDown,
                   .onMouseMove = SharedShapeOnMouseMove,
                   .onMouseUp = SharedShapeOnMouseUp,
                   .drawOverlay = ShapeToolDrawOverlay,
                   TOOL_LIFECYCLE_SHAPE_COMMON},
    [TOOL_CURVE] = {BezierToolOnMouseDown, BezierToolOnMouseMove,
                    BezierToolOnMouseUp, NULL, BezierToolDrawOverlay,
                    NULL, BezierTool_Deactivate, BezierTool_Cancel, NULL, IsCurvePending, NULL},
    [TOOL_RECT] = {.onMouseDown = SharedShapeOnMouseDown,
                   .onMouseMove = SharedShapeOnMouseMove,
                   .onMouseUp = SharedShapeOnMouseUp,
                   .drawOverlay = ShapeToolDrawOverlay,
                   TOOL_LIFECYCLE_SHAPE_COMMON},
    [TOOL_POLYGON] = {PolygonTool_OnMouseDown, PolygonTool_OnMouseMove,
                      PolygonTool_OnMouseUp, PolygonTool_OnDoubleClick,
                      PolygonTool_DrawOverlay, NULL, PolygonTool_Deactivate,
                      PolygonTool_Cancel, NULL, IsPolygonPending, NULL},
    [TOOL_ELLIPSE] = {.onMouseDown = SharedShapeOnMouseDown,
                      .onMouseMove = SharedShapeOnMouseMove,
                      .onMouseUp = SharedShapeOnMouseUp,
                      .drawOverlay = ShapeToolDrawOverlay,
                      TOOL_LIFECYCLE_SHAPE_COMMON},
    [TOOL_ROUNDRECT] = {.onMouseDown = SharedShapeOnMouseDown,
                        .onMouseMove = SharedShapeOnMouseMove,
                        .onMouseUp = SharedShapeOnMouseUp,
                        .drawOverlay = ShapeToolDrawOverlay,
                        TOOL_LIFECYCLE_SHAPE_COMMON},
    [TOOL_PEN] = {.onMouseDown = PenToolOnMouseDown,
                  .onMouseMove = PenToolOnMouseMove,
                  .onMouseUp = PenToolOnMouseUp,
                  TOOL_LIFECYCLE_MODAL_COMMON(PenTool_Deactivate,
                                              CancelPenDrawing, IsPenDrawing,
                                              NULL)},
    [TOOL_HIGHLIGHTER] = {
        .onMouseDown = HighlighterToolOnMouseDown,
        .onMouseMove = HighlighterToolOnMouseMove,
        .onMouseUp = HighlighterToolOnMouseUp,
        TOOL_LIFECYCLE_MODAL_COMMON(HighlighterTool_Deactivate,
                                    CancelHighlighterDrawing,
                                    IsHighlighterDrawing,
                                    NULL)},
    [TOOL_CRAYON] = {.onMouseDown = CrayonToolOnMouseDown,
                     .onMouseMove = CrayonToolOnMouseMove,
                     .onMouseUp = CrayonToolOnMouseUp,
                     TOOL_LIFECYCLE_MODAL_COMMON(CrayonTool_Deactivate,
                                                 CancelCrayonDrawing,
                                                 IsCrayonDrawing,
                                                 NULL)}};

static const ToolVTable *GetToolVTable(int toolId) {
  if (toolId < 0 ||
      toolId >= (int)(sizeof(s_ToolTable) / sizeof(s_ToolTable[0]))) {
    return NULL;
  }
  if (!s_ToolTable[toolId].onMouseDown && !s_ToolTable[toolId].onMouseMove &&
      !s_ToolTable[toolId].onMouseUp && !s_ToolTable[toolId].onDoubleClick &&
      !s_ToolTable[toolId].drawOverlay && !s_ToolTable[toolId].onViewportChanged) {
    return NULL;
  }
  return &s_ToolTable[toolId];
}

/*------------------------------------------------------------
   Modal Tool Commit/Cancel System
   Explicit checks replace the former CommittableTool table.
  ------------------------------------------------------------*/

void ToolCancel(void) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());

  HWND hCap = s_runtime.capturedWindow;
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  if (tool && tool->onCancel) {
    tool->onCancel();
  }

  if (hCap || GetCapture() == GetCanvasWindow()) {
    ReleaseCapture();
  }

  InvalidateCanvas();
}

void ToolCancelSkipSelection(void) {
  int t = Tool_GetCurrent();
  HWND hc = GetCanvasWindow();

  if ((t == TOOL_SELECT || t == TOOL_FREEFORM) && SelectionIsDragging() && hc) {
    s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;
    SelectionToolOnMouseUp(hc, 0, 0, 0);
  }

  HWND hCap = s_runtime.capturedWindow;
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  if (t != TOOL_SELECT && t != TOOL_FREEFORM) {
    const ToolVTable *tool = GetToolVTable(t);
    if (tool && tool->onCancel) {
      tool->onCancel();
    }
  }

  if (hCap || (hc && GetCapture() == hc)) {
    ReleaseCapture();
  }

  InvalidateCanvas();
}

static void ToolOnCaptureLost(void) {
  // If hCapturedWindow is NULL, it means we voluntarily released capture in ToolOnMouseUp.
  // In this case, we don't want to cancel the tool's state (e.g. selection/text box).
  if (s_runtime.capturedWindow == NULL) {
    return;
  }

  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  {
    int activeStrokeTool = StrokeSession_GetActiveToolId();
    if (activeStrokeTool >= 0) {
      const char *action = (activeStrokeTool == TOOL_LINE || activeStrokeTool == TOOL_RECT ||
                            activeStrokeTool == TOOL_ELLIPSE || activeStrokeTool == TOOL_ROUNDRECT)
                               ? "Draw Shape" : "Draw";
      StrokeSession_OnActiveCaptureLost(action);
    }
  }

  ToolCancel(); /* abort any in-progress tool that never got MouseUp */
}

void ResetToolStateForNewDocument(void) {
  // Cancel any modal tool state and clear transient interactions
  ToolCancel();
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
    ToolCancel();
    break;
  case TOOL_LIFECYCLE_CANCEL_SKIP_SELECTION:
    ToolCancelSkipSelection();
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
  case TOOL_LIFECYCLE_TIMER_TICK: {
    const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
    if (tool && tool->onTimerTick) tool->onTimerTick();
    break;
  }
  default:
    break;
  }
}

/*------------------------------------------------------------
   Public Tool API Functions
  ------------------------------------------------------------*/

int GetCurrentTool(void) { return Tool_GetCurrent(); }

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

  const ToolVTable *tool = GetToolVTable(activeTool);
  if (tool && tool->onMouseDown) {
    tool->onMouseDown(hWnd, x, y, nButton);
  }

  // Track if this tool captured the mouse
  if (GetCapture() == hWnd) s_runtime.capturedWindow = hWnd;
}

static void ToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);
  const ToolVTable *tool = GetToolVTable(activeTool);
  if (tool && tool->onMouseMove) {
    tool->onMouseMove(hWnd, x, y, nButton);
  }
}

static void ToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);
  const ToolVTable *tool = GetToolVTable(activeTool);

  // Always clear active-tool-at-mouse-down on button up so modifier keys
  // work correctly for the next operation. Relying on GetCapture() can leave
  // state stale if a tool does not capture or capture is lost.
  // Clearing BEFORE dispatching prevents voluntary ReleaseCapture() inside
  // tools from triggering an abort via WM_CAPTURECHANGED.
  s_runtime.activeToolAtMouseDown = -1; s_runtime.capturedWindow = NULL;

  if (tool && tool->onMouseUp) {
    tool->onMouseUp(hWnd, x, y, nButton);
  }
}

static void ToolOnDoubleClick(HWND hWnd, int x, int y, int nButton) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->onDoubleClick) {
    tool->onDoubleClick(hWnd, x, y, nButton);
  }
}

void ToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->drawOverlay) {
    tool->drawOverlay(hdc, dScale, nDestX, nDestY);
  }
}

void Tool_FinalizeCurrentState(void) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->onDeactivate) {
    tool->onDeactivate();
  }
}

void SetCurrentTool(int nTool) {
  if (currentTool == nTool)
    return;

  if (s_runtime.capturedWindow != NULL) {
    ToolCancel();
  }

  /* Finalize the outgoing tool */
  Tool_FinalizeCurrentState();

  int oldTool = Tool_GetCurrent();
  currentTool = nTool;

  /* Activate the incoming tool */
  const ToolVTable *newVT = GetToolVTable(nTool);
  if (newVT && newVT->onActivate) {
    newVT->onActivate();
  }

  UpdateToolOptions(nTool);
  HWND hToolbar = GetToolbarWindow();
  if (hToolbar)
    InvalidateWindow(hToolbar);
}

static void ToolOnViewportChanged(HWND hWnd) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->onViewportChanged) {
    tool->onViewportChanged(hWnd);
  }
}

int Tool_GetCurrent(void) { return currentTool; }
