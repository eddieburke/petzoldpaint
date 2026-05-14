#ifndef SHAPE_SESSION_H
#define SHAPE_SESSION_H

#include "tool_session.h"

typedef ToolSession ShapeSession;

void ShapeSession_Begin(ShapeSession *s, HWND hWnd, int x, int y, int nButton, int toolId);
void ShapeSession_UpdateLastPoint(ShapeSession *s, int x, int y);
void ShapeSession_MarkModified(ShapeSession *s);
void ShapeSession_CommitIfNeeded(ShapeSession *s, const char *actionName);
void ShapeSession_End(ShapeSession *s);
void ShapeSession_Cancel(ShapeSession *s);

#endif
