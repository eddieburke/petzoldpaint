#ifndef STROKE_SESSION_H
#define STROKE_SESSION_H

#include "tool_session.h"

typedef ToolSession StrokeSession;

void StrokeSession_Begin(StrokeSession *s, HWND hWnd, int x, int y, int nButton, int toolId);
void StrokeSession_UpdateLastPoint(StrokeSession *s, int x, int y);
BOOL StrokeSession_IsActiveButton(int nButton);
void StrokeSession_MarkPixelsModified(StrokeSession *s);
void StrokeSession_MarkModified(StrokeSession *s);
void StrokeSession_SetModified(StrokeSession *s);
void StrokeSession_Invalidate(void);
void StrokeSession_CommitIfNeeded(StrokeSession *s, const char *actionName);
void StrokeSession_End(StrokeSession *s);
void StrokeSession_Cancel(StrokeSession *s);
void StrokeSession_OnCaptureLost(StrokeSession *s, const char *actionName);
void StrokeSession_OnActiveCaptureLost(const char *actionName);
int StrokeSession_GetActiveToolId(void);

#endif
