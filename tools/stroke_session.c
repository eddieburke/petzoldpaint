#include "stroke_session.h"
#include "../helpers.h"
#include "../history.h"

static StrokeSession *s_activeSession = NULL;

void StrokeSession_Begin(StrokeSession *s, HWND hWnd, int x, int y, int nButton, int toolId) {
  if (!s) return;
  s->isDrawing = TRUE;
  s->pixelsModified = FALSE;
  s->drawButton = nButton;
  s->toolId = toolId;
  s->lastPoint.x = x;
  s->lastPoint.y = y;
  s_activeSession = s;
  SetCapture(hWnd);
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
void StrokeSession_MarkModified(StrokeSession *s) { StrokeSession_MarkPixelsModified(s); }
void StrokeSession_SetModified(StrokeSession *s) { StrokeSession_MarkPixelsModified(s); }

void StrokeSession_Invalidate(void) { InvalidateCanvas(); }

void StrokeSession_CommitIfNeeded(StrokeSession *s, const char *actionName) {
  if (!s || !s->isDrawing || !s->pixelsModified) return;
  HistoryPushToolActionById(s->toolId, actionName ? actionName : "Draw");
}

void StrokeSession_End(StrokeSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  if (s_activeSession == s) s_activeSession = NULL;
  ReleaseCapture();
  SetDocumentDirty();
}

void StrokeSession_Cancel(StrokeSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  if (s_activeSession == s) s_activeSession = NULL;
  ReleaseCapture();
  InvalidateCanvas();
}

void StrokeSession_OnCaptureLost(StrokeSession *s, const char *actionName) {
  StrokeSession_CommitIfNeeded(s, actionName);
}

void StrokeSession_OnActiveCaptureLost(const char *actionName) {
  StrokeSession_OnCaptureLost(s_activeSession, actionName);
}

int StrokeSession_GetActiveToolId(void) {
  return (s_activeSession && s_activeSession->isDrawing) ? s_activeSession->toolId : -1;
}
