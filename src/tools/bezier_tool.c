#include "bezier_tool.h"
#include "canvas.h"
#include "draw.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "overlay.h"
#include "commit_bar.h"
#include "tools.h"
#include "interaction.h"
#include "tool_options/tool_options.h"
#include <stdlib.h>

typedef enum {
  STATE_IDLE = 0,
  STATE_DRAWING_LINE,
  STATE_WAIT_CTRL1,
  STATE_DRAG_CTRL1,
  STATE_WAIT_CTRL2,
  STATE_DRAG_CTRL2,
  STATE_EDITING,
  STATE_DRAG_EDIT_HANDLE
} BezierState;

static BezierState state = STATE_IDLE;
static POINT ptStart;
static POINT ptEnd;
static POINT ptCtrl1;
static POINT ptCtrl2;
static int nDrawButton = 0;
static BOOL bSuspendingCapture = FALSE;
static int nEditHandle = -1;

static int BezierHitHandle(int x, int y) {
  int tol = (int)(8.0 / GetZoomScale());
  if (tol < 2)
    tol = 2;
  POINT pts[4] = {ptStart, ptEnd, ptCtrl1, ptCtrl2};
  for (int i = 0; i < 4; i++) {
    int dx = x - pts[i].x;
    int dy = y - pts[i].y;
    if (dx * dx + dy * dy <= tol * tol)
      return i;
  }
  return -1;
}

static void BezierGetBounds(RECT *outRect) {
  if (!outRect)
    return;
  POINT pts[4] = {ptStart, ptEnd, ptCtrl1, ptCtrl2};
  *outRect = GetBoundingBox(pts, 4);
}

static void DrawBezierSegment(BYTE *bits, int width, int height, COLORREF color,
                              BYTE alpha, int thickness) {
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
    DrawLineStampCircles(bits, width, height, prevX, prevY, x, y, radius, color,
                    alpha, LAYER_BLEND_NORMAL);
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
  BYTE alpha = GetOpacityForButton(nDrawButton);
  int thickness = (nBrushWidth > 0) ? nBrushWidth : 1;
  DrawBezierSegment(bits, Canvas_GetWidth(), Canvas_GetHeight(), color, alpha, thickness);
  LayersMarkDirty();
}

static void BezierReset(void) {
  state = STATE_IDLE;
  bSuspendingCapture = FALSE;
  nEditHandle = -1;
  LayersClearDraft();
}

static void BezierCommitCurve(void) {
  if (state == STATE_IDLE)
    return;
  UpdateDraftLayer();
  Interaction_Commit("Draw Curve");
  BezierReset();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
void BezierTool_CommitPending(void) { BezierCommitCurve(); }

BOOL BezierTool_Cancel(void) {
  if (bSuspendingCapture)
    return FALSE;
  Interaction_Abort();
  BezierReset();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  return TRUE;
}

void BezierTool_Deactivate(void) {
  if (state != STATE_IDLE) {
    Interaction_Abort();
    BezierReset();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  }
}

BOOL IsCurvePending(void) { return state != STATE_IDLE; }

void BezierTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (nButton == MK_RBUTTON) {
    if (state != STATE_IDLE) BezierTool_Cancel();
    return;
  }
  if (nButton != MK_LBUTTON) return;

  switch (state) {
  case STATE_IDLE:
    nDrawButton = nButton;
    ptStart.x = x;
    ptStart.y = y;
    ptEnd = ptCtrl1 = ptCtrl2 = ptStart;
    state = STATE_DRAWING_LINE;
    Interaction_BeginEx(hWnd, x, y, nButton, TOOL_CURVE, FALSE);
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
  case STATE_EDITING: {
    RECT rcBounds;
    BezierGetBounds(&rcBounds);
    COMMIT_BAR_HANDLE_CLICK(&rcBounds, x, y, BezierCommitCurve(),
                            BezierTool_Cancel());

    nEditHandle = BezierHitHandle(x, y);
    if (nEditHandle >= 0) {
      state = STATE_DRAG_EDIT_HANDLE;
      SetCapture(hWnd);
    } else {
      BezierCommitCurve();
      return;
    }
    break;
  }
  default:
    break;
  }
  UpdateDraftLayer();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void BezierTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
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
  case STATE_DRAG_EDIT_HANDLE:
    if (nEditHandle == 0) {
      ptStart.x = x; ptStart.y = y;
    } else if (nEditHandle == 1) {
      ptEnd.x = x; ptEnd.y = y;
    } else if (nEditHandle == 2) {
      ptCtrl1.x = x; ptCtrl1.y = y;
    } else if (nEditHandle == 3) {
      ptCtrl2.x = x; ptCtrl2.y = y;
    }
    break;
  default: break;
  }
  UpdateDraftLayer();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void BezierTool_OnMouseUp(HWND hWnd, int x, int y, int nButton) {
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
    ptCtrl2.x = x;
    ptCtrl2.y = y;
    state = STATE_EDITING;
    HistoryPushSession("Create Curve");
    break;
  case STATE_DRAG_EDIT_HANDLE:
    if (nEditHandle == 0) {
      ptStart.x = x; ptStart.y = y;
    } else if (nEditHandle == 1) {
      ptEnd.x = x; ptEnd.y = y;
    } else if (nEditHandle == 2) {
      ptCtrl1.x = x; ptCtrl1.y = y;
    } else if (nEditHandle == 3) {
      ptCtrl2.x = x; ptCtrl2.y = y;
    }
    nEditHandle = -1;
    state = STATE_EDITING;
    HistoryPushSession("Adjust Curve");
    break;
  default: break;
  }
  UpdateDraftLayer();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

void BezierTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY) {
  if (state == STATE_IDLE) return;
  OverlayContext ctx;
  Overlay_Init(&ctx, hdc, dScale, nDestX, nDestY);
  Overlay_DrawHandle(&ctx, ptStart.x, ptStart.y, OVERLAY_HANDLE_CIRCLE, FALSE);
  Overlay_DrawHandle(&ctx, ptEnd.x, ptEnd.y, OVERLAY_HANDLE_CIRCLE, FALSE);
  if (state >= STATE_DRAG_CTRL1)
    Overlay_DrawHandle(&ctx, ptCtrl1.x, ptCtrl1.y, OVERLAY_HANDLE_CIRCLE, TRUE);
  if (state >= STATE_DRAG_CTRL2)
    Overlay_DrawHandle(&ctx, ptCtrl2.x, ptCtrl2.y, OVERLAY_HANDLE_CIRCLE, TRUE);
  if (state == STATE_EDITING || state == STATE_DRAG_EDIT_HANDLE) {
    RECT rcBounds;
    BezierGetBounds(&rcBounds);
    CommitBar_Draw(&ctx, &rcBounds);
  }
}

BezierToolSnapshot *BezierTool_CreateSnapshot(void) {
  if (state == STATE_IDLE)
    return NULL;
  BezierToolSnapshot *snapshot =
      (BezierToolSnapshot *)calloc(1, sizeof(BezierToolSnapshot));
  if (!snapshot)
    return NULL;
  snapshot->state = state;
  snapshot->ptStart = ptStart;
  snapshot->ptEnd = ptEnd;
  snapshot->ptCtrl1 = ptCtrl1;
  snapshot->ptCtrl2 = ptCtrl2;
  snapshot->drawButton = nDrawButton;
  return snapshot;
}

void BezierTool_DestroySnapshot(BezierToolSnapshot *snapshot) {
  free(snapshot);
}

void BezierTool_ApplySnapshot(const BezierToolSnapshot *snapshot) {
  if (Interaction_IsActive())
    Interaction_EndQuiet();
  BezierReset();

  if (!snapshot) {
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return;
  }

  state = (BezierState)snapshot->state;
  if (state == STATE_DRAG_CTRL1 || state == STATE_DRAG_CTRL2 ||
      state == STATE_DRAG_EDIT_HANDLE)
    state = STATE_EDITING;
  ptStart = snapshot->ptStart;
  ptEnd = snapshot->ptEnd;
  ptCtrl1 = snapshot->ptCtrl1;
  ptCtrl2 = snapshot->ptCtrl2;
  nDrawButton = snapshot->drawButton;
  nEditHandle = -1;

  if (!Interaction_IsActive()) {
    Interaction_BeginEx(GetCanvasWindow(), ptStart.x, ptStart.y, nDrawButton,
                        TOOL_CURVE, FALSE);
  }

  UpdateDraftLayer();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
