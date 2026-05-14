/*------------------------------------------------------------------------------
 * BEZIER_TOOL.C
 *
 * Bezier Curve Drawing Tool Implementation (Draft Layer model)
 *
 * State Machine:
 * 1. STATE_DRAWING_LINE : Drag to define P0 (start) and P3 (end).
 * 2. STATE_WAIT_CTRL1   : Click/drag to position P1.
 * 3. STATE_DRAG_CTRL1   : Dragging P1.
 * 4. STATE_WAIT_CTRL2   : Click/drag to position P2.
 * 5. STATE_DRAG_CTRL2   : Dragging P2 — on MouseUp the curve is committed.
 *----------------------------------------------------------------------------*/

#include "bezier_tool.h"
#include "../canvas.h"
#include "../draw.h"
#include "../geom.h"
#include "../helpers.h"
#include "../history.h"
#include "../layers.h"
#include "../overlay.h"
#include "../resource.h"
#include "../tools.h"
#include "tool_options/tool_options.h"

typedef enum {
  STATE_IDLE = 0,
  STATE_DRAWING_LINE,
  STATE_WAIT_CTRL1,
  STATE_DRAG_CTRL1,
  STATE_WAIT_CTRL2,
  STATE_DRAG_CTRL2
} BezierState;

static BezierState state = STATE_IDLE;
static POINT ptStart;
static POINT ptEnd;
static POINT ptCtrl1;
static POINT ptCtrl2;
static int nDrawButton = 0;
static BOOL bSuspendingCapture = FALSE;

static void DrawBezierSegment(BYTE *bits, int width, int height, COLORREF color,
                              int thickness) {
  POINT p0 = ptStart, p1 = ptCtrl1, p2 = ptCtrl2, p3 = ptEnd;
  int steps = 100;
  int dist = abs(p0.x - p3.x) + abs(p0.y - p3.y);
  if (dist < 50) steps = 20;
  float radius = (thickness > 0 ? thickness : 1) / 2.0f;
  float prec = 1.0f / steps;
  float prevX = (float)p0.x;
  float prevY = (float)p0.y;
  for (int i = 1; i <= steps; i++) {
    float t = i * prec;
    float u = 1.0f - t;
    float u2 = u * u, u3 = u2 * u;
    float t2 = t * t, t3 = t2 * t;
    float x = u3 * p0.x + 3 * u2 * t * p1.x + 3 * u * t2 * p2.x + t3 * p3.x;
    float y = u3 * p0.y + 3 * u2 * t * p1.y + 3 * u * t2 * p2.y + t3 * p3.y;
    DrawLineAAAlpha(bits, width, height, prevX, prevY, x, y, radius, color,
                    255, LAYER_BLEND_NORMAL);
    prevX = x;
    prevY = y;
  }
}

static void UpdateDraftLayer(void) {
  if (state == STATE_IDLE) return;
  LayersClearDraft();
  BYTE *bits = LayersGetDraftBits();
  if (!bits) return;
  COLORREF color = GetColorForButton(nDrawButton);
  int thickness = (nBrushWidth > 0) ? nBrushWidth : 1;
  DrawBezierSegment(bits, Canvas_GetWidth(), Canvas_GetHeight(), color, thickness);
  LayersMarkDirty();
}

static void BezierReset(void) {
  state = STATE_IDLE;
  bSuspendingCapture = FALSE;
  LayersClearDraft();
}

void CommitPendingCurve(void) {
  if (state == STATE_IDLE) return;
  UpdateDraftLayer();
  LayersMergeDraftToActive();
  LayersMarkDirty();
  SetDocumentDirty();
  HistoryPushToolActionById(TOOL_CURVE, "Draw Curve");
  BezierReset();
  InvalidateCanvas();
}

BOOL BezierTool_Cancel(void) {
  if (bSuspendingCapture) return FALSE;
  BezierReset();
  InvalidateCanvas();
  return TRUE;
}

void BezierTool_Deactivate(void) {
  if (state != STATE_IDLE) {
    BezierReset();
    InvalidateCanvas();
  }
}

BOOL IsCurvePending(void) { return state != STATE_IDLE; }

void BezierToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (nButton == MK_RBUTTON) {
    if (state != STATE_IDLE) BezierTool_Cancel();
    return;
  }
  if (nButton != MK_LBUTTON) return;

  switch (state) {
  case STATE_IDLE:
    nDrawButton = nButton;
    ptStart.x = x; ptStart.y = y;
    ptEnd = ptCtrl1 = ptCtrl2 = ptStart;
    state = STATE_DRAWING_LINE;
    SetCapture(hWnd);
    break;
  case STATE_WAIT_CTRL1:
    ptCtrl1.x = x; ptCtrl1.y = y;
    state = STATE_DRAG_CTRL1;
    SetCapture(hWnd);
    break;
  case STATE_WAIT_CTRL2:
    ptCtrl2.x = x; ptCtrl2.y = y;
    state = STATE_DRAG_CTRL2;
    SetCapture(hWnd);
    break;
  default:
    break;
  }
  UpdateDraftLayer();
  InvalidateCanvas();
}

void BezierToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (GetCapture() != hWnd) return;
  switch (state) {
  case STATE_DRAWING_LINE:
    ptEnd.x = x; ptEnd.y = y;
    ptCtrl1 = ptStart; ptCtrl2 = ptEnd;
    if (IsShiftDown()) {
      SnapToAngle(ptStart.x, ptStart.y, &ptEnd.x, &ptEnd.y, SNAP_ANGLE_DEG);
      ptCtrl2 = ptEnd;
    }
    break;
  case STATE_DRAG_CTRL1:
    ptCtrl1.x = x; ptCtrl1.y = y;
    break;
  case STATE_DRAG_CTRL2:
    ptCtrl2.x = x; ptCtrl2.y = y;
    break;
  default: break;
  }
  UpdateDraftLayer();
  InvalidateCanvas();
}

void BezierToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (GetCapture() != hWnd) return;
  bSuspendingCapture = TRUE;
  ReleaseCapture();
  bSuspendingCapture = FALSE;

  switch (state) {
  case STATE_DRAWING_LINE:
    ptEnd.x = x; ptEnd.y = y;
    if (IsShiftDown())
      SnapToAngle(ptStart.x, ptStart.y, &ptEnd.x, &ptEnd.y, SNAP_ANGLE_DEG);
    ptCtrl1 = ptStart; ptCtrl2 = ptEnd;
    state = STATE_WAIT_CTRL1;
    break;
  case STATE_DRAG_CTRL1:
    ptCtrl1.x = x; ptCtrl1.y = y;
    state = STATE_WAIT_CTRL2;
    break;
  case STATE_DRAG_CTRL2:
    ptCtrl2.x = x; ptCtrl2.y = y;
    CommitPendingCurve();
    return;
  default: break;
  }
  UpdateDraftLayer();
  InvalidateCanvas();
}

void BezierToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  if (state == STATE_IDLE) return;
  OverlayContext ctx;
  Overlay_Init(&ctx, hdc, dScale, nDestX, nDestY);
  Overlay_DrawHandle(&ctx, ptStart.x, ptStart.y, OVERLAY_HANDLE_CIRCLE, FALSE);
  Overlay_DrawHandle(&ctx, ptEnd.x, ptEnd.y, OVERLAY_HANDLE_CIRCLE, FALSE);
  if (state >= STATE_DRAG_CTRL1)
    Overlay_DrawHandle(&ctx, ptCtrl1.x, ptCtrl1.y, OVERLAY_HANDLE_CIRCLE, TRUE);
  if (state >= STATE_DRAG_CTRL2)
    Overlay_DrawHandle(&ctx, ptCtrl2.x, ptCtrl2.y, OVERLAY_HANDLE_CIRCLE, TRUE);
}
