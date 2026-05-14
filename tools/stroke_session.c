#include "stroke_session.h"
#include "../helpers.h"

void StrokeSession_Begin(StrokeSession *s, HWND hWnd, int x, int y, int nButton, int toolId) {
  ToolSession_Begin(s, hWnd, x, y, nButton, toolId);
}

void StrokeSession_UpdateLastPoint(StrokeSession *s, int x, int y) { ToolSession_UpdateLastPoint(s, x, y); }
BOOL StrokeSession_IsActiveButton(int nButton) { return ToolSession_IsActiveButton(nButton); }
void StrokeSession_MarkPixelsModified(StrokeSession *s) { ToolSession_MarkModified(s); }
void StrokeSession_MarkModified(StrokeSession *s) { ToolSession_MarkModified(s); }
void StrokeSession_SetModified(StrokeSession *s) { ToolSession_MarkModified(s); }
void StrokeSession_Invalidate(void) { InvalidateCanvas(); }
void StrokeSession_CommitIfNeeded(StrokeSession *s, const char *actionName) { ToolSession_CommitIfNeeded(s, actionName); }
void StrokeSession_End(StrokeSession *s) { ToolSession_End(s); }
void StrokeSession_Cancel(StrokeSession *s) { ToolSession_Cancel(s); }
void StrokeSession_OnCaptureLost(StrokeSession *s, const char *actionName) { ToolSession_OnCaptureLost(s, actionName); }
void StrokeSession_OnActiveCaptureLost(const char *actionName) { ToolSession_OnActiveCaptureLost(actionName); }
int StrokeSession_GetActiveToolId(void) { return ToolSession_GetActiveToolId(); }
