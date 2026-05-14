#include "tool_session.h"
#include "../helpers.h"
#include "../history.h"

static ToolSession *s_activeSession = NULL;

void ToolSession_Begin(ToolSession *s, HWND hWnd, int x, int y, int button, int toolId) {
  if (!s) return;
  s->isDrawing = TRUE;
  s->pixelsModified = FALSE;
  s->drawButton = button;
  s->toolId = toolId;
  s->lastPoint.x = x;
  s->lastPoint.y = y;
  s_activeSession = s;
  SetCapture(hWnd);
}

void ToolSession_UpdateLastPoint(ToolSession *s, int x, int y) {
  if (!s) return;
  s->lastPoint.x = x;
  s->lastPoint.y = y;
}

void ToolSession_MarkModified(ToolSession *s) {
  if (s) s->pixelsModified = TRUE;
}

void ToolSession_CommitIfNeeded(ToolSession *s, const char *actionName) {
  if (!s || !s->isDrawing || !s->pixelsModified) return;
  HistoryPushToolActionById(s->toolId, actionName ? actionName : "Draw");
}

void ToolSession_End(ToolSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  if (s_activeSession == s) s_activeSession = NULL;
  ReleaseCapture();
  SetDocumentDirty();
}

void ToolSession_Cancel(ToolSession *s) {
  if (!s || !s->isDrawing) return;
  s->isDrawing = FALSE;
  if (s_activeSession == s) s_activeSession = NULL;
  ReleaseCapture();
  InvalidateCanvas();
}

BOOL ToolSession_IsActiveButton(int nButton) { return (nButton & (MK_LBUTTON | MK_RBUTTON)) != 0; }


void ToolSession_OnCaptureLost(ToolSession *s, const char *actionName) {
  ToolSession_CommitIfNeeded(s, actionName);
}

void ToolSession_OnActiveCaptureLost(const char *actionName) {
  ToolSession_OnCaptureLost(s_activeSession, actionName);
}

int ToolSession_GetActiveToolId(void) {
  return (s_activeSession && s_activeSession->isDrawing) ? s_activeSession->toolId : -1;
}
