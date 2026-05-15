#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "peztold_core.h"


void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId);


void AirbrushTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void AirbrushTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void AirbrushTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);
void FreehandTool_OnTimerTick(void);


BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
BOOL FreehandTool_Cancel(void);
int GetActiveFreehandTool(void);

#endif
