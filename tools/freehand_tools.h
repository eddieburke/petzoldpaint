#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Shared Freehand Stroke Controller API
 *----------------------------------------------------------------------------*/

typedef struct FreehandStrokePolicy FreehandStrokePolicy;

const FreehandStrokePolicy *FreehandGetPolicyForTool(int tool);
void BeginStroke(HWND hWnd, int x, int y, int nButton, const FreehandStrokePolicy *policy);
void AppendPoint(HWND hWnd, int x, int y, int nButton);
void EndStroke(HWND hWnd, int x, int y, int nButton);
void CancelStroke(int reason);

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
