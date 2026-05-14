#include "stroke_session.h"
#include "../helpers.h"
#include "../history.h"
#include "capture_guard.h"

void StrokeSession_Begin(StrokeSession *s, HWND hWnd, int x, int y, int nButton, int toolId) {
  if (!s) return;
  s->isDrawing = TRUE;
  s->pixelsModified = FALSE;
  s->drawButton = nButton;
  s->toolId = toolId;
  s->lastPoint.x = x;
  s->lastPoint.y = y;
  CaptureBegin(hWnd, toolId, nButton);
}

void StrokeSession_UpdateLastPoint(StrokeSession *s, int x, int y) {
  if (!s) return;
  s->lastPoint.x = x;
  s->lastPoint.y = y;
}

BOOL StrokeSession_IsActiveButton(int nButton) { return (nButton & (MK_LBUTTON | MK_RBUTTON)) != 0; }

void StrokeSession_MarkPixelsModified(StrokeSession *s) {
  if (s) s->pixelsModified = TRUE;
}

void StrokeSession_Invalidate(void) { InvalidateCanvas(); }

void StrokeSession_CommitIfNeeded(StrokeSession *s, const char *actionName) {
  if (!s || !s->isDrawing || !s->pixelsModified) return;
  HistoryPushToolActionById(s->toolId, actionName ? actionName : "Draw");
}

void StrokeSession_End(StrokeSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  CaptureEnd(NULL, s->toolId, CAPTURE_END_NORMAL);
  SetDocumentDirty();
}

void StrokeSession_Cancel(StrokeSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  CaptureEnd(NULL, s->toolId, CAPTURE_END_CANCEL);
  InvalidateCanvas();
}

void StrokeSession_OnCaptureLost(StrokeSession *s, const char *actionName) {
  if (!s || !s->isDrawing) return;
  CaptureOnLost(NULL, s->toolId);
  StrokeSession_CommitIfNeeded(s, actionName);
  s->isDrawing = FALSE;
}
