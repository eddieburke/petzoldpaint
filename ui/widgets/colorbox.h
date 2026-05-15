#ifndef COLORBOX_H
#define COLORBOX_H
#include <windows.h>

#define COLORBOX_HEIGHT 72
void CreateColorbox(HWND hParent);
void ColorboxOnPaint(HWND hWnd);
void ColorboxOnLButtonDown(HWND hWnd, int x, int y);
void ColorboxOnRButtonDown(HWND hWnd, int x, int y);
void ColorboxOnDoubleClick(HWND hWnd, int x, int y);
void ColorboxSyncCustomColors(void);
HWND GetColorboxWindow(void);
#endif
