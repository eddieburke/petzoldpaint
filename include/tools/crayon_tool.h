#ifndef CRAYON_TOOL_H
#define CRAYON_TOOL_H

#include "peztold_core.h"


void CrayonTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void CrayonTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void CrayonTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);

BOOL IsCrayonDrawing(void);
void CrayonTool_Deactivate(void);
BOOL CrayonTool_Cancel(void);


void CrayonOptions_Draw(HDC hdc, RECT *prc);
BOOL CrayonOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL CrayonOptions_MouseMove(HWND hwnd, int y);

void CrayonTool_RegisterPresets(void);

#endif
