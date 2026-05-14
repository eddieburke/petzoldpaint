#ifndef MAGNIFIER_TOOL_H
#define MAGNIFIER_TOOL_H

#include <windows.h>

void MagnifierToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void MagnifierToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void MagnifierToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void MagnifierToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);
void MagnifierToolDeactivate(void);

#endif // MAGNIFIER_TOOL_H
