#ifndef CRAYON_TOOL_H
#define CRAYON_TOOL_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Crayon Tool API
 *----------------------------------------------------------------------------*/

void CrayonToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void CrayonToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void CrayonToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

BOOL IsCrayonDrawing(void);
void CrayonTool_Deactivate(void);
BOOL CancelCrayonDrawing(void);

/*------------------------------------------------------------------------------
 * Tool Options (Owned by Crayon Tool)
 *----------------------------------------------------------------------------*/

void CrayonOptions_Draw(HDC hdc, RECT *prc);
BOOL CrayonOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL CrayonOptions_MouseMove(HWND hwnd, int y);

void CrayonTool_RegisterPresets(void);

#endif
