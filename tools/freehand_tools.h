#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Shared Freehand Stroke Controller API
 *----------------------------------------------------------------------------*/

void FreehandTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId);
void FreehandTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId);

/*------------------------------------------------------------------------------
 * Airbrush Tool
 *----------------------------------------------------------------------------*/

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void FreehandTool_OnTimerTick(void);

/*------------------------------------------------------------------------------
 * Shared State Accessors
 *----------------------------------------------------------------------------*/

BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
BOOL CancelFreehandDrawing(void);
int GetActiveFreehandTool(void);

#endif
