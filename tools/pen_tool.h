#ifndef PEN_TOOL_H
#define PEN_TOOL_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Pen Tool
 *----------------------------------------------------------------------------*/

void PenToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void PenToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void PenToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * State Accessors
 *----------------------------------------------------------------------------*/

BOOL IsPenDrawing(void);
void PenTool_Deactivate(void);
BOOL CancelPenDrawing(void);

#endif
