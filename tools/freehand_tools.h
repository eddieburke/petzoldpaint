#ifndef FREEHAND_TOOLS_H
#define FREEHAND_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Pencil Tool
 *----------------------------------------------------------------------------*/

void PencilToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void PencilToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void PencilToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Brush Tool
 *----------------------------------------------------------------------------*/

void BrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void BrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void BrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Eraser Tool
 *----------------------------------------------------------------------------*/

void EraserToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void EraserToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void EraserToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Airbrush Tool
 *----------------------------------------------------------------------------*/

void AirbrushToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void AirbrushToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void AirbrushToolTrigger(HWND hWnd);

/*------------------------------------------------------------------------------
 * Shared State Accessors
 *----------------------------------------------------------------------------*/

BOOL IsFreehandDrawing(void);
void FreehandTool_Deactivate(void);
void FreehandTool_OnCaptureLost(void);
BOOL CancelFreehandDrawing(void);
int GetActiveFreehandTool(void);

#endif
