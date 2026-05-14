#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"
#include "airbrush_tool.h"

void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId);

/* Legacy dispatch hook kept so tools.c can continue routing timer events. */
void FreehandTool_OnTimerTick(void);

BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
BOOL CancelFreehandDrawing(void);
int GetActiveFreehandTool(void);

#endif
