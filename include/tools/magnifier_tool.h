#ifndef MAGNIFIER_TOOL_H
#define MAGNIFIER_TOOL_H

#include <windows.h>

void MagnifierTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void MagnifierTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void MagnifierTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);
void MagnifierTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);
void MagnifierTool_Deactivate(void);

#endif
