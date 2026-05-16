#ifndef PEN_TOOL_H
#define PEN_TOOL_H

#include "peztold_core.h"


void PenTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void PenTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void PenTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);


BOOL IsPenDrawing(void);
void PenTool_Deactivate(void);
BOOL PenTool_Cancel(void);

#endif
