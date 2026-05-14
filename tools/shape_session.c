#include "shape_session.h"

void ShapeSession_Begin(ShapeSession *s, HWND hWnd, int x, int y, int nButton, int toolId) {
  ToolSession_Begin(s, hWnd, x, y, nButton, toolId);
}

void ShapeSession_UpdateLastPoint(ShapeSession *s, int x, int y) {
  ToolSession_UpdateLastPoint(s, x, y);
}

void ShapeSession_MarkModified(ShapeSession *s) { ToolSession_MarkModified(s); }
void ShapeSession_CommitIfNeeded(ShapeSession *s, const char *actionName) { ToolSession_CommitIfNeeded(s, actionName); }
void ShapeSession_End(ShapeSession *s) { ToolSession_End(s); }
void ShapeSession_Cancel(ShapeSession *s) { ToolSession_Cancel(s); }
