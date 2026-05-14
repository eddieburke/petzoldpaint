#ifndef TOOL_SESSION_H
#define TOOL_SESSION_H

#include "../peztold_core.h"

typedef struct {
  BOOL isDrawing;
  BOOL pixelsModified;
  int drawButton;
  int toolId;
  POINT lastPoint;
} ToolSession;

void ToolSession_Begin(ToolSession *s, HWND hWnd, int x, int y, int button, int toolId);
void ToolSession_UpdateLastPoint(ToolSession *s, int x, int y);
void ToolSession_MarkModified(ToolSession *s);
void ToolSession_CommitIfNeeded(ToolSession *s, const char *actionName);
void ToolSession_End(ToolSession *s);
void ToolSession_Cancel(ToolSession *s);
BOOL ToolSession_IsActiveButton(int nButton);
void ToolSession_OnCaptureLost(ToolSession *s, const char *actionName);
void ToolSession_OnActiveCaptureLost(const char *actionName);
int ToolSession_GetActiveToolId(void);

#endif
