#ifndef TOOLBAR_H
#define TOOLBAR_H
#include <windows.h>

#define TOOLBAR_WIDTH 56
void CreateToolbar(HWND hParent);
void ToolbarOnPaint(HWND hWnd);
void ToolbarOnLButtonDown(HWND hWnd, int x, int y);
void ToolbarOnMouseMove(HWND hWnd, int x, int y);
void ToolbarOnLButtonUp(HWND hWnd, int x, int y);
void ToolbarOnLButtonUp(HWND hWnd, int x, int y);
HWND GetToolbarWindow();
int GetToolbarHeight(void);
#endif
