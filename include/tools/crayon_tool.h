#ifndef CRAYON_TOOL_H
#define CRAYON_TOOL_H
#include "peztold_core.h"
#include "tools.h"
void CrayonTool_OnPointer(const ToolPointerEvent *ev);
BOOL IsCrayonDrawing(void);
void CrayonTool_Deactivate(void);
BOOL CrayonTool_Cancel(void);
void CrayonOptions_Draw(HDC hdc, RECT *prc);
BOOL CrayonOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL CrayonOptions_MouseMove(HWND hwnd, int y);
void CrayonTool_RegisterPresets(void);
#endif
