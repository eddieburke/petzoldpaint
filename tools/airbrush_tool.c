/*------------------------------------------------------------------------------
 * AIRBRUSH_TOOL.C
 *
 * Timer-driven spray tool. Kept separate from freehand tools on purpose: holding
 * still should keep spraying, while pencil/brush/eraser only draw from movement.
 *----------------------------------------------------------------------------*/

#include "airbrush_tool.h"
#include "../canvas.h"
#include "../geom.h"
#include "../helpers.h"
#include "../layers.h"
#include "drawing_primitives.h"
#include "stroke_session.h"
#include "tool_options/tool_options.h"

static StrokeSession s_session = {0};

static void InvalidateSprayPoint(int x, int y) {
  int radius = DrawPrim_GetSprayRadius(nSprayRadius) + 2;
  RECT rcDirty = {x - radius, y - radius, x + radius, y + radius};
  RECT rcScreen;

  LayersMarkDirtyRect(&rcDirty);
  RectBmpToScr(&rcDirty, &rcScreen);
  InflateRect(&rcScreen, 2, 2);
  InvalidateCanvasRect(&rcScreen);
}

static void SprayAtCurrentPoint(void) {
  BYTE *bits = LayersGetActiveColorBits();
  if (!bits)
    return;

  DrawPrim_DrawSprayPoint(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                          s_session.lastPoint.x, s_session.lastPoint.y,
                          GetColorForButton(s_session.drawButton),
                          nSprayRadius);
  InvalidateSprayPoint(s_session.lastPoint.x, s_session.lastPoint.y);
  StrokeSession_MarkPixelsModified(&s_session);
}

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  if (s_session.isDrawing && nButton != s_session.drawButton) {
    CancelAirbrushDrawing();
    return;
  }

  StrokeSession_Begin(&s_session, hWnd, x, y, nButton, TOOL_AIRBRUSH);
  SprayAtCurrentPoint();
  SetTimer(hWnd, TIMER_AIRBRUSH, 30, NULL);
}

void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!s_session.isDrawing || !StrokeSession_IsActiveButton(nButton))
    return;

  StrokeSession_UpdateLastPoint(&s_session, x, y);
  SprayAtCurrentPoint();
}

void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  if (!s_session.isDrawing)
    return;

  if (StrokeSession_IsActiveButton(nButton)) {
    StrokeSession_UpdateLastPoint(&s_session, x, y);
    SprayAtCurrentPoint();
  }

  KillTimer(hWnd, TIMER_AIRBRUSH);
  StrokeSession_CommitIfNeeded(&s_session, "Airbrush");
  StrokeSession_End(&s_session);
}

void AirbrushToolOnTimerTick(HWND hWnd) {
  (void)hWnd;
  if (s_session.isDrawing)
    SprayAtCurrentPoint();
}

BOOL IsAirbrushDrawing(void) { return s_session.isDrawing; }

void AirbrushTool_Deactivate(void) {
  KillTimer(GetCanvasWindow(), TIMER_AIRBRUSH);
  StrokeSession_End(&s_session);
}

BOOL CancelAirbrushDrawing(void) {
  if (!s_session.isDrawing)
    return FALSE;

  KillTimer(GetCanvasWindow(), TIMER_AIRBRUSH);
  StrokeSession_Cancel(&s_session);
  return TRUE;
}
