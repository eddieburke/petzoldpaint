#ifndef TOOLS_H
#define TOOLS_H
#include "peztold_core.h"
#include "tools/selection_tool.h"

void ToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void ToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void ToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void ToolOnDoubleClick(HWND hWnd, int x, int y, int nButton);
void ToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);
void SetCurrentTool(int nTool);
int GetCurrentTool(void);
int Tool_GetCurrent(void);
void Tool_FinalizeCurrentState(void);
void InitializeTools(void);
void CommitCurrentSelection(void);
void ClearSelection(void);
void ToolTriggerAirbrush(HWND hWnd);
void ToolCancel(void);
void ToolCancelSkipSelection(void);
void ToolOnCaptureLost(void);
void ToolOnViewportChanged(HWND hWnd);
void ResetToolStateForNewDocument(void);
#endif
