#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Shared Freehand Tool Entry Points
 *----------------------------------------------------------------------------*/

void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int tool);
void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int tool);
void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int tool);
void FreehandTool_OnTimerTick(void);

/*------------------------------------------------------------------------------
 * Shared State Accessors
 *----------------------------------------------------------------------------*/

BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
void FreehandTool_OnCaptureLost(void);
BOOL CancelFreehandDrawing(void);
int GetActiveFreehandTool(void);

#endif
