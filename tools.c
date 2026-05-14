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

static int nActiveToolAtMouseDown =
    -1; // Tool that was active when mouse was pressed
static HWND hCapturedWindow = NULL; // Window that has mouse capture

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
} ToolVTable;

static const ToolVTable *GetToolVTable(int toolId);

/*------------------------------------------------------------
   Tool Resolution Functions with Keypress State Management
  ------------------------------------------------------------*/

// Resolve tool for mouse down - allows temporary tool switch
static int ResolveActiveToolForMouseDown(void) {
  // Only allow Alt for eyedropper if no tool has capture
  if (IsAltDown() && GetCapture() == NULL) {
    return TOOL_PICK;
  }
  return Tool_GetCurrent();
}

// Resolve tool for mouse move/up - respects captured state
static int ResolveActiveToolForMoveUp(HWND hWnd) {
  // If we have an active tool from mouse down, use that regardless of modifier
  // keys. This prevents Alt-key eyedropper from interfering with active mouse
  // operations.
  if (hWnd && GetCapture() == hWnd && nActiveToolAtMouseDown >= 0) {
    return nActiveToolAtMouseDown;
  }

  // If no capture, allow Alt to temporarily switch to Pick only when no tool
  // is in a drawing/pending state. Otherwise we would switch mid-stroke.
  if (IsAltDown()) {
    const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
    if (tool && tool->isBusy && tool->isBusy()) {
      return Tool_GetCurrent();
    }
    return TOOL_PICK;
  }

  return Tool_GetCurrent();
}



/*------------------------------------------------------------
   Tool VTable Registry
  ------------------------------------------------------------*/

static const ToolVTable s_ToolTable[] = {
    /* {onMouseDown, onMouseMove, onMouseUp, onDoubleClick,
         drawOverlay, onActivate, onDeactivate, onCancel} */
    [TOOL_FREEFORM] = {SelectionToolOnMouseDown, SelectionToolOnMouseMove,
                       SelectionToolOnMouseUp, NULL,
                       SelectionToolDrawOverlay, NULL, SelectionTool_Deactivate,
                       SelectionTool_Cancel},
    [TOOL_SELECT] = {SelectionToolOnMouseDown, SelectionToolOnMouseMove,
                      SelectionToolOnMouseUp, NULL,
                      SelectionToolDrawOverlay, NULL, SelectionTool_Deactivate,
                      SelectionTool_Cancel},

    [TOOL_ERASER] = {EraserToolOnMouseDown, EraserToolOnMouseMove,
                      EraserToolOnMouseUp, NULL, NULL, NULL,
                      FreehandTool_Deactivate,
                      CancelFreehandDrawing, NULL, IsFreehandDrawing, FreehandTool_OnCaptureLost},
    [TOOL_FILL] = {FillToolOnMouseDown, NULL, NULL, NULL, NULL, NULL,
                    NULL},
    [TOOL_PICK] = {PickToolOnMouseDown, PickToolOnMouseMove, NULL, NULL, NULL,
                    NULL, NULL, NULL},
    [TOOL_MAGNIFIER] = {MagnifierToolOnMouseDown, MagnifierToolOnMouseMove,
                        MagnifierToolOnMouseUp, NULL,
                        MagnifierToolDrawOverlay, NULL, MagnifierToolDeactivate,
                        NULL, NULL, NULL, MagnifierToolDeactivate},
    [TOOL_PENCIL] = {PencilToolOnMouseDown, PencilToolOnMouseMove,
                     PencilToolOnMouseUp, NULL, NULL, NULL,
                     FreehandTool_Deactivate,
                     CancelFreehandDrawing, NULL, IsFreehandDrawing, FreehandTool_OnCaptureLost},
    [TOOL_BRUSH] = {BrushToolOnMouseDown, BrushToolOnMouseMove,
                    BrushToolOnMouseUp, NULL, NULL, NULL,
                    FreehandTool_Deactivate,
                    CancelFreehandDrawing, NULL, IsFreehandDrawing, FreehandTool_OnCaptureLost},
    [TOOL_AIRBRUSH] = {AirbrushToolOnMouseDown, AirbrushToolOnMouseMove,
                       AirbrushToolOnMouseUp, NULL, NULL, NULL,
                       FreehandTool_Deactivate,
                       CancelFreehandDrawing, NULL, IsFreehandDrawing, FreehandTool_OnCaptureLost},
    [TOOL_TEXT] = {TextToolOnMouseDown, TextToolOnMouseMove, TextToolOnMouseUp,
                   NULL, TextToolDrawOverlay, NULL,
                   TextTool_Deactivate, CancelText, TextToolOnViewportChanged},

    /* Shape tools: draft layer model — no ghost, lifecycle hooks registered */
    [TOOL_LINE] = {LineToolOnMouseDown, LineToolOnMouseMove, LineToolOnMouseUp,
                   NULL, ShapeToolDrawOverlay, NULL, ShapeTool_Deactivate,
                   ShapeTool_Cancel, NULL, ShapeTool_IsBusy, NULL},
    [TOOL_CURVE] = {BezierToolOnMouseDown, BezierToolOnMouseMove,
                    BezierToolOnMouseUp, NULL, BezierToolDrawOverlay,
                    NULL, BezierTool_Deactivate, BezierTool_Cancel, NULL, IsCurvePending, NULL},
    [TOOL_RECT] = {RectToolOnMouseDown, RectToolOnMouseMove, RectToolOnMouseUp,
                   NULL, ShapeToolDrawOverlay, NULL, ShapeTool_Deactivate,
                   ShapeTool_Cancel, NULL, ShapeTool_IsBusy, NULL},
    [TOOL_POLYGON] = {PolygonTool_OnMouseDown, PolygonTool_OnMouseMove,
                      PolygonTool_OnMouseUp, PolygonTool_OnDoubleClick,
                      PolygonTool_DrawOverlay, NULL, PolygonTool_Deactivate,
                      PolygonTool_Cancel, NULL, IsPolygonPending, NULL},
    [TOOL_ELLIPSE] = {EllipseToolOnMouseDown, EllipseToolOnMouseMove,
                      EllipseToolOnMouseUp, NULL, ShapeToolDrawOverlay,
                      NULL, ShapeTool_Deactivate, ShapeTool_Cancel, NULL, ShapeTool_IsBusy, NULL},
    [TOOL_ROUNDRECT] = {RoundRectToolOnMouseDown, RoundRectToolOnMouseMove,
                      RoundRectToolOnMouseUp, NULL,
                      ShapeToolDrawOverlay, NULL, ShapeTool_Deactivate,
                      ShapeTool_Cancel, NULL, ShapeTool_IsBusy, NULL},
    [TOOL_PEN] = {PenToolOnMouseDown, PenToolOnMouseMove, PenToolOnMouseUp,
                  NULL, NULL, NULL, PenTool_Deactivate,
                  CancelPenDrawing, NULL, IsPenDrawing, PenTool_OnCaptureLost},
    [TOOL_HIGHLIGHTER] = {HighlighterToolOnMouseDown,
                          HighlighterToolOnMouseMove, HighlighterToolOnMouseUp,
                          NULL, NULL, NULL, HighlighterTool_Deactivate,
                          CancelHighlighterDrawing, NULL, IsHighlighterDrawing, HighlighterTool_OnCaptureLost},
    [TOOL_CRAYON] = {CrayonToolOnMouseDown, CrayonToolOnMouseMove,
                     CrayonToolOnMouseUp, NULL, NULL, NULL,
                     CrayonTool_Deactivate, CancelCrayonDrawing, NULL, IsCrayonDrawing, CrayonTool_OnCaptureLost}};

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

  HWND hCap = hCapturedWindow;
  hCapturedWindow = NULL;
  nActiveToolAtMouseDown = -1;

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
    hCapturedWindow = NULL;
    nActiveToolAtMouseDown = -1;
    SelectionToolOnMouseUp(hc, 0, 0, 0);
  }

  HWND hCap = hCapturedWindow;
  hCapturedWindow = NULL;
  nActiveToolAtMouseDown = -1;

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

void ToolOnCaptureLost(void) {
  // If hCapturedWindow is NULL, it means we voluntarily released capture in ToolOnMouseUp.
  // In this case, we don't want to cancel the tool's state (e.g. selection/text box).
  if (hCapturedWindow == NULL) {
    return;
  }

  nActiveToolAtMouseDown = -1;
  hCapturedWindow = NULL;

  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->onCaptureLost) {
    tool->onCaptureLost();
  }

  ToolCancel(); /* abort any in-progress tool that never got MouseUp */
}

void ResetToolStateForNewDocument(void) {
  // Cancel any modal tool state and clear transient interactions
  ToolCancel();
}

/*------------------------------------------------------------
   Public Tool API Functions
  ------------------------------------------------------------*/

int GetCurrentTool(void) { return Tool_GetCurrent(); }

void InitializeTools(void) {
  currentTool = TOOL_PENCIL;
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

void ToolTriggerAirbrush(HWND hWnd) {
  if (currentTool == TOOL_AIRBRUSH && IsFreehandDrawing()) {
    AirbrushToolTrigger(hWnd);
  }
}

void ToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMouseDown();

  // Store the active tool and window at mouse down to prevent state conflicts
  // This ensures that if the user presses Alt during a drag, we don't switch
  // to the eyedropper mid-operation
  nActiveToolAtMouseDown = activeTool;
  hCapturedWindow = NULL; // Will be set by SetCapture in tool handlers

  const ToolVTable *tool = GetToolVTable(activeTool);
  if (tool && tool->onMouseDown) {
    tool->onMouseDown(hWnd, x, y, nButton);
  }

  // Track if this tool captured the mouse
  if (GetCapture() == hWnd) {
    hCapturedWindow = hWnd;
  }
}

void ToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);
  const ToolVTable *tool = GetToolVTable(activeTool);
  if (tool && tool->onMouseMove) {
    tool->onMouseMove(hWnd, x, y, nButton);
  }
}

void ToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  int activeTool = ResolveActiveToolForMoveUp(hWnd);
  const ToolVTable *tool = GetToolVTable(activeTool);

  // Always clear active-tool-at-mouse-down on button up so modifier keys
  // work correctly for the next operation. Relying on GetCapture() can leave
  // state stale if a tool does not capture or capture is lost.
  // Clearing BEFORE dispatching prevents voluntary ReleaseCapture() inside
  // tools from triggering an abort via WM_CAPTURECHANGED.
  nActiveToolAtMouseDown = -1;
  hCapturedWindow = NULL;

  if (tool && tool->onMouseUp) {
    tool->onMouseUp(hWnd, x, y, nButton);
  }
}

void ToolOnDoubleClick(HWND hWnd, int x, int y, int nButton) {
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

  if (hCapturedWindow != NULL) {
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

void ToolOnViewportChanged(HWND hWnd) {
  const ToolVTable *tool = GetToolVTable(Tool_GetCurrent());
  if (tool && tool->onViewportChanged) {
    tool->onViewportChanged(hWnd);
  }
}

int Tool_GetCurrent(void) { return currentTool; }
