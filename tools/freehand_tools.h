#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Shared Freehand Stroke Controller API
 *----------------------------------------------------------------------------*/

void FreehandOnMouseDown(HWND hWnd, int x, int y, int nButton, int tool);
void FreehandOnMouseMove(HWND hWnd, int x, int y, int nButton, int tool);
void FreehandOnMouseUp(HWND hWnd, int x, int y, int nButton, int tool);

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
void FreehandTool_OnCaptureLost(void);
BOOL CancelFreehandDrawing(void);
int GetActiveFreehandTool(void);

#endif
