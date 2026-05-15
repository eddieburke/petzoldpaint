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
#include "../history.h"
#include "../layers.h"
#include "../overlay.h"
#include "../commit_bar.h"
#include "../tools.h"
#include "tool_options/tool_options.h"
#include "palette.h"
#include "interaction.h"
#include <stdlib.h>

/*------------------------------------------------------------------------------
 * Tool State Definitions
 *----------------------------------------------------------------------------*/

typedef enum {
  SHAPE_STATE_IDLE = 0, // No active shape
  SHAPE_STATE_CREATING, // User dragging to create shape
  SHAPE_STATE_EDITING,  // Created and waiting for commit
  SHAPE_STATE_RESIZING  // Dragging handles/body
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
  int nHandle;
  POINT ptDragStart;
  POINT ptStartOrig;
  POINT ptEndOrig;
} s_Shape = {SHAPE_STATE_IDLE, 0, {0, 0}, {0, 0}, 0};

/*------------------------------------------------------------------------------
 * DrawShapeToBits
 *
 * Core renderer — writes the current shape into the given pixel buffer.
 * Used for both the live draft preview and the final merge-to-active.
 *----------------------------------------------------------------------------*/

static void DrawShapeToBits(BYTE *bits, int width, int height) {
  COLORREF fg = GetColorForButton(s_Shape.drawButton);
  COLORREF bg = (s_Shape.drawButton == MK_RBUTTON) ? Palette_GetPrimaryColor() : Palette_GetSecondaryColor();
  BYTE fgAlpha = GetOpacityForButton(s_Shape.drawButton);
  BYTE bgAlpha = (s_Shape.drawButton == MK_RBUTTON) ? Palette_GetPrimaryOpacity() : Palette_GetSecondaryOpacity();
  int w = (nBrushWidth > 0 ? nBrushWidth : 1);

  switch (s_Shape.activeToolId) {
  case TOOL_LINE:
    if (nBrushWidth <= 1) {
      DrawLineAlpha(bits, width, height, s_Shape.ptStart.x, s_Shape.ptStart.y,
                    s_Shape.ptEnd.x, s_Shape.ptEnd.y, 1, fg, fgAlpha, LAYER_BLEND_NORMAL);
    } else {
      DrawLineAAAlpha(bits, width, height, (float)s_Shape.ptStart.x,
                      (float)s_Shape.ptStart.y, (float)s_Shape.ptEnd.x,
                      (float)s_Shape.ptEnd.y, nBrushWidth / 2.0f, fg, fgAlpha, LAYER_BLEND_NORMAL);
    }
    break;
 
  case TOOL_RECT: {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    if (nShapeDrawType == 1 || nShapeDrawType == 2) {
      COLORREF fill = (nShapeDrawType == 1) ? fg : bg;
      BYTE fillAlpha = (nShapeDrawType == 1) ? fgAlpha : bgAlpha;
      DrawRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, fill, fillAlpha, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawRectOutlineAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg,
                           fgAlpha, w, LAYER_BLEND_NORMAL);
    break;
  }
 
  case TOOL_ELLIPSE: {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    int rw = rc.right - rc.left;
    int rh = rc.bottom - rc.top;
    if (nShapeDrawType == 1 || nShapeDrawType == 2) {
      COLORREF fill = (nShapeDrawType == 1) ? fg : bg;
      BYTE fillAlpha = (nShapeDrawType == 1) ? fgAlpha : bgAlpha;
      DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fill,
                       fillAlpha, TRUE, 0, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg, fgAlpha,
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
      BYTE fillAlpha = (nShapeDrawType == 1) ? fgAlpha : bgAlpha;
      DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, radius,
                           fill, fillAlpha, TRUE, 0, LAYER_BLEND_NORMAL);
    }
    if (nShapeDrawType == 0 || nShapeDrawType == 2)
      DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, radius,
                           fg, fgAlpha, FALSE, w, LAYER_BLEND_NORMAL);
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
  s_Shape.nHandle = HT_NONE;
}

static void CommitShapeAction(const char *actionName) {
  Interaction_Commit(actionName ? actionName : "Draw Shape");
  ResetShapeState();
  InvalidateCanvas();
}

/* Called when tool is deactivated (tool switch, layer op, etc.) */
static void ShapeTool_OnDeactivate(void) {
  if (s_Shape.state != SHAPE_STATE_IDLE) {
    LayersClearDraft();
    Interaction_Abort();
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

void CancelShapeDrawing(void) { ShapeTool_OnCancel(); }
void ShapeTool_CommitPending(void) {
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return;
  UpdateDraftLayer();
  Interaction_MarkModified();
  CommitShapeAction("Draw Shape");
}

/*------------------------------------------------------------------------------
 * Event Handlers
 *----------------------------------------------------------------------------*/

void ShapeTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId) {
  if (nButton == MK_RBUTTON && s_Shape.state != SHAPE_STATE_IDLE) {
    ShapeTool_OnCancel();
    return;
  }

  if (s_Shape.state == SHAPE_STATE_IDLE) {
    LayersClearDraft();

    s_Shape.state = SHAPE_STATE_CREATING;
    s_Shape.activeToolId = toolId;
    s_Shape.drawButton = nButton;
    s_Shape.ptStart.x = x;
    s_Shape.ptStart.y = y;
    s_Shape.ptEnd = s_Shape.ptStart;
    Interaction_Begin(hWnd, x, y, nButton, toolId);
    return;
  }

  if (s_Shape.activeToolId != toolId)
    return;

  if (s_Shape.state == SHAPE_STATE_EDITING) {
    RECT rcBounds = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                      s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    COMMIT_BAR_HANDLE_CLICK(&rcBounds, x, y, ShapeTool_CommitPending(),
                            ShapeTool_OnCancel());

    int hit = Overlay_HitTestBoxHandles(&rcBounds, x, y);
    if (hit != HT_NONE) {
      s_Shape.state = SHAPE_STATE_RESIZING;
      s_Shape.nHandle = hit;
      s_Shape.ptDragStart.x = x;
      s_Shape.ptDragStart.y = y;
      s_Shape.ptStartOrig = s_Shape.ptStart;
      s_Shape.ptEndOrig = s_Shape.ptEnd;
      SetCapture(hWnd);
      return;
    }

    ShapeTool_CommitPending();
    return;
  }
}

void ShapeTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId) {
  (void)hWnd;
  (void)nButton;
  if (s_Shape.state == SHAPE_STATE_IDLE || s_Shape.activeToolId != toolId)
    return;

  if (s_Shape.state == SHAPE_STATE_CREATING) {
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
      Interaction_MarkModified();
      InvalidateCanvas();
    }
    return;
  }

  if (s_Shape.state == SHAPE_STATE_RESIZING && GetCapture() == hWnd) {
    int dx = x - s_Shape.ptDragStart.x;
    int dy = y - s_Shape.ptDragStart.y;

    RECT rc = GetRectFromPoints(s_Shape.ptStartOrig.x, s_Shape.ptStartOrig.y,
                                s_Shape.ptEndOrig.x, s_Shape.ptEndOrig.y);
    if (s_Shape.nHandle == HT_BODY) {
      OffsetRect(&rc, dx, dy);
    } else {
      ResizeRect(&rc, s_Shape.nHandle, dx, dy, 2, IsShiftDown());
      NormalizeRect(&rc);
    }

    s_Shape.ptStart.x = rc.left;
    s_Shape.ptStart.y = rc.top;
    s_Shape.ptEnd.x = rc.right;
    s_Shape.ptEnd.y = rc.bottom;
    UpdateDraftLayer();
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
    UpdateDraftLayer();
    Interaction_MarkModified();
    s_Shape.state = SHAPE_STATE_EDITING;
    HistoryPushToolSessionById(toolId, "Create Shape");
    return;
  }

  if (s_Shape.state == SHAPE_STATE_RESIZING && s_Shape.activeToolId == toolId) {
    if (GetCapture() == hWnd)
      ReleaseCapture();
    s_Shape.state = SHAPE_STATE_EDITING;
    s_Shape.nHandle = HT_NONE;
    UpdateDraftLayer();
    HistoryPushToolSessionById(toolId, "Adjust Shape");
    InvalidateCanvas();
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

  if (s_Shape.state == SHAPE_STATE_EDITING) {
    RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y,
                                s_Shape.ptEnd.x, s_Shape.ptEnd.y);
    CommitBar_Draw(&ctx, &rc);
  }
}

/* Lifecycle hooks — wired into ToolVTable in tools.c */
void ShapeTool_Deactivate(void) { ShapeTool_OnDeactivate(); }
BOOL ShapeTool_Cancel(void) { return ShapeTool_OnCancel(); }

ShapeToolSnapshot *ShapeTool_CreateSnapshot(void) {
  if (s_Shape.state == SHAPE_STATE_IDLE)
    return NULL;

  ShapeToolSnapshot *snapshot =
      (ShapeToolSnapshot *)calloc(1, sizeof(ShapeToolSnapshot));
  if (!snapshot)
    return NULL;
  snapshot->state = s_Shape.state;
  snapshot->activeToolId = s_Shape.activeToolId;
  snapshot->ptStart = s_Shape.ptStart;
  snapshot->ptEnd = s_Shape.ptEnd;
  snapshot->drawButton = s_Shape.drawButton;
  return snapshot;
}

void ShapeTool_DestroySnapshot(ShapeToolSnapshot *snapshot) { free(snapshot); }

void ShapeTool_ApplySnapshot(const ShapeToolSnapshot *snapshot) {
  if (s_Shape.state != SHAPE_STATE_IDLE) {
    LayersClearDraft();
    if (Interaction_IsActive())
      Interaction_EndQuiet();
  }
  ResetShapeState();

  if (!snapshot) {
    InvalidateCanvas();
    return;
  }

  s_Shape.state = (ShapeState)snapshot->state;
  s_Shape.activeToolId = snapshot->activeToolId;
  s_Shape.ptStart = snapshot->ptStart;
  s_Shape.ptEnd = snapshot->ptEnd;
  s_Shape.drawButton = snapshot->drawButton;
  s_Shape.nHandle = HT_NONE;

  if (!Interaction_IsActive()) {
    Interaction_BeginEx(GetCanvasWindow(), s_Shape.ptStart.x, s_Shape.ptStart.y,
                        s_Shape.drawButton, s_Shape.activeToolId, FALSE);
  }

  UpdateDraftLayer();
  InvalidateCanvas();
}
