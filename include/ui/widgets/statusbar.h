#ifndef STATUSBAR_H
#define STATUSBAR_H
#include <windows.h>
#define STATUSBAR_HEIGHT 22
void CreateStatusBar(HWND hParent);
HWND GetStatusBarWindow(void);
void StatusBarSetCoordinates(int x, int y);
void StatusBarSetColor(COLORREF color);
void StatusBarSetVisible(BOOL bVisible);
BOOL StatusBarIsVisible(void);
int StatusBarGetHeight(void);
void StatusBarUpdateZoom(double z);
#endif
