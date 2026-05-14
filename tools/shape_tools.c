/*------------------------------------------------------------------------------
 * SHAPE_TOOLS.C
 *
 * Shape Drawing Tools Implementation
 *
 * Implements the standard vector-like shape tools: Line, Rectangle, Ellipse,
 * and Rounded Rectangle.
 *
 * Architecture (Draft Layer model):
 * - MouseDown : Record start point, clear draft layer.
 * - MouseMove : Clear draft, redraw current shape to draft layer.
 * - MouseUp   : Merge draft to active layer, push history, reset state.
 * - onDeactivate / onCancel : Clear draft, reset state.
 *
 * No ghost/preview-buffer callbacks needed — the compositor renders the
 * draft layer above the active layer automatically every frame.
 *----------------------------------------------------------------------------*/

#include "shape_tools.h"
#include "../canvas.h"
#include "../draw.h"
#include "../geom.h"
#include "../helpers.h"
#include "../layers.h"
#include "../overlay.h"
#include "../tools.h"
#include "../ui/widgets/colorbox.h"
#include "tool_options/tool_options.h"
#include "palette.h"
#include "tool_session.h"

/*------------------------------------------------------------------------------
 * Tool State Definitions
 *----------------------------------------------------------------------------*/

typedef enum {
  SHAPE_STATE_IDLE = 0, // No active shape
  SHAPE_STATE_CREATING  // User dragging to create shape
} ShapeState;

/*------------------------------------------------------------------------------
 * Internal State Variables
 *----------------------------------------------------------------------------*/

static struct {
  ShapeState state;
  int activeToolId;
  POINT ptStart;
  POINT ptEnd;
  int drawButton;
} s_Shape = {SHAPE_STATE_IDLE, 0, {0, 0}, {0, 0}, 0};
static ToolSession s_shapeSession = {0};

/*------------------------------------------------------------------------------
 * DrawShapeToBits
 *
 * Core renderer — writes the current shape into the given pixel buffer.
 * Used for both the live draft preview and the final merge-to-active.
 *----------------------------------------------------------------------------*/

static void DrawShapeToBits(BYTE *bits, int width, int height) {
  COLORREF fg = GetColorForButton(s_Shape.drawButton);
  COLORREF bg = (s_Shape.drawButton == MK_RBUTTON) ? Palette_GetPrimaryColor() : Palette_GetSecondaryColor();
  BYTE alpha = 255;
  int w = (nBrushWidth > 0 ? nBrushWidth : 1);

  switch (s_Shape.activeToolId) {
  case TOOL_LINE:
    if (nBrushWidth <= 1) {
      DrawLineAlpha(bits, width, height, s_Shape.ptStart.x, s_Shape.ptStart.y,
                    s_Shape.ptEnd.x, s_Shape.ptEnd.y, 1, fg, alpha, LAYER_BLEND_NORMAL);
    } else {
      DrawLineAAAlpha(bits, width, height, (float)s_Shape.ptStart.x,
                      (float)s_Shape.ptStart.y, (float)s_Shape.ptEnd.x,
                      (float)s_Shape.ptEnd.y, nBrushWidth / 2.0f, fg, alpha, LAYER_BLEND_NORMAL);
    }
    break;
 
  case TOOL_RECT: {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    if (nShapeDrawType == 1 || nShapeDrawType == 2) {
      COLORREF fill = (nShapeDrawType == 1) ? fg : bg;
      DrawRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, fill, alpha, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawRectOutlineAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg,
                           alpha, w, LAYER_BLEND_NORMAL);
    break;
  }
 
  case TOOL_ELLIPSE: {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    if (nShapeDrawType == 1 || nShapeDrawType == 2) {
      COLORREF fill = (nShapeDrawType == 1) ? fg : bg;
      DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fill,
                       alpha, TRUE, 0, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg, alpha,
                       FALSE, w, LAYER_BLEND_NORMAL);
    break;
  }
 
  case TOOL_ROUNDRECT: {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    int radius = 16;
    if (nShapeDrawType == 1 || nShapeDrawType == 2) {
      COLORREF fill = (nShapeDrawType == 1) ? fg : bg;
      DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, radius,
                           fill, alpha, TRUE, 0, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, radius,
                           fg, alpha, FALSE, w, LAYER_BLEND_NORMAL);
    break;
  }
  }
}

/*------------------------------------------------------------------------------
 * UpdateDraftLayer
 *
 * Clears the draft and redraws the current shape into it.
 * Called every MouseMove so the compositor shows a live preview.
 *----------------------------------------------------------------------------*/

static void UpdateDraftLayer(void) {
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return;

  LayersClearDraft();

  BYTE *bits = LayersGetDraftBits();
  if (!bits)
    return;

  DrawShapeToBits(bits, Canvas_GetWidth(), Canvas_GetHeight());
  LayersMarkDirty();
}

/*------------------------------------------------------------------------------
 * State Management
 *----------------------------------------------------------------------------*/

static void ResetShapeState(void) {
  s_Shape.state = SHAPE_STATE_IDLE;
  s_Shape.activeToolId = 0;
}

static void CommitShapeAction(int toolId, const char *actionName) {
  LayersMergeDraftToActive();
  LayersMarkDirty();
  ToolSession_CommitIfNeeded(&s_shapeSession, actionName);
  ToolSession_End(&s_shapeSession);
  ResetShapeState();
  InvalidateCanvas();
}

/* Called when tool is deactivated (tool switch, layer op, etc.) */
static void ShapeTool_OnDeactivate(void) {
  if (s_Shape.state != SHAPE_STATE_IDLE) {
    LayersClearDraft();
    ToolSession_Cancel(&s_shapeSession);
    ResetShapeState();
    InvalidateCanvas();
  }
}

/* Called on explicit cancel (Escape key, right-click cancel) */
static BOOL ShapeTool_OnCancel(void) {
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return FALSE;
  ShapeTool_OnDeactivate();
  return TRUE;
}

BOOL IsShapeDrawing(void) { return s_Shape.state == SHAPE_STATE_CREATING; }
BOOL IsShapePending(void) { return s_Shape.state != SHAPE_STATE_IDLE; }

BOOL ShapeTool_IsBusy(void) {
  return IsShapeDrawing() || IsShapePending();
}

/* Legacy compat — called by CommitCurrentTool fallback path */
void CommitPendingShape(void) {
  if (!IsShapePending())
    return;
  /* Merge whatever is in the draft to active */
  CommitShapeAction(s_Shape.activeToolId, "Draw Shape");
}

void CancelShapeDrawing(void) { ShapeTool_OnCancel(); }

/*------------------------------------------------------------------------------
 * Event Handlers
 *----------------------------------------------------------------------------*/

void ShapeTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId) {
  if (s_Shape.state != SHAPE_STATE_IDLE)
    return;

  LayersClearDraft();

  s_Shape.state = SHAPE_STATE_CREATING;
  s_Shape.activeToolId = toolId;
  s_Shape.drawButton = nButton;
  s_Shape.ptStart.x = x;
  s_Shape.ptStart.y = y;
  s_Shape.ptEnd = s_Shape.ptStart;
  ToolSession_Begin(&s_shapeSession, hWnd, x, y, nButton, toolId);
}

void ShapeTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId) {
  (void)hWnd;
  (void)nButton;
  if (s_Shape.state == SHAPE_STATE_IDLE || s_Shape.activeToolId != toolId)
    return;

  int endX = x, endY = y;
  if (IsShiftDown()) {
    if (toolId == TOOL_LINE)
      SnapToAngle(s_Shape.ptStart.x, s_Shape.ptStart.y, &endX, &endY, 45);
    else
      SnapToSquare(s_Shape.ptStart.x, s_Shape.ptStart.y, &endX, &endY);
  }

  if (s_Shape.ptEnd.x != endX || s_Shape.ptEnd.y != endY) {
    s_Shape.ptEnd.x = endX;
    s_Shape.ptEnd.y = endY;
    UpdateDraftLayer();
    ToolSession_MarkModified(&s_shapeSession);
    InvalidateCanvas();
  }
}

void ShapeTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId) {
  (void)hWnd;
  (void)nButton;
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return;
  if (s_Shape.state == SHAPE_STATE_CREATING && s_Shape.activeToolId == toolId) {
    int endX = x, endY = y;
    if (IsShiftDown()) {
      if (toolId == TOOL_LINE)
        SnapToAngle(s_Shape.ptStart.x, s_Shape.ptStart.y, &endX, &endY, 45);
      else
        SnapToSquare(s_Shape.ptStart.x, s_Shape.ptStart.y, &endX, &endY);
    }
    s_Shape.ptEnd.x = endX;
    s_Shape.ptEnd.y = endY;

    /* Draw final shape directly to draft, then merge into active layer */
    UpdateDraftLayer();
    ToolSession_MarkModified(&s_shapeSession);
    CommitShapeAction(toolId, "Draw Shape");
  }
}

/*------------------------------------------------------------------------------
 * Overlay Drawing (screen-space handles — unchanged)
 *----------------------------------------------------------------------------*/

void ShapeToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return;

  OverlayContext ctx;
  Overlay_Init(&ctx, hdc, dScale, nDestX, nDestY);

  if (s_Shape.activeToolId == TOOL_LINE) {
    POINT pts[2] = {s_Shape.ptStart, s_Shape.ptEnd};
    Overlay_DrawPolyHandles(&ctx, pts, 2);
  } else {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    Overlay_DrawBoxHandles(&ctx, &rc);
  }
}

/* Lifecycle hooks — wired into ToolVTable in tools.c */
void ShapeTool_Deactivate(void) { ShapeTool_OnDeactivate(); }
BOOL ShapeTool_Cancel(void) { return ShapeTool_OnCancel(); }
