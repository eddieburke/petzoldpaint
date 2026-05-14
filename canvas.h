#ifndef CANVAS_H
#define CANVAS_H
#include "peztold_core.h"

BOOL CreateCanvas(int width, int height);
void ResizeCanvas(int newWidth, int newHeight);
void DestroyCanvas(void);
void UpdateCanvas(HDC hdc, int x, int y, int w, int h);
void ClearCanvas(COLORREF color);
BOOL Undo(void);
BOOL Redo(void);

void CreateCanvasWindow(HWND hParent);
HWND GetCanvasWindow(void);
void ResizeCanvasWindow(int x, int y, int w, int h);
void RefreshCanvasRect(RECT* pRect);
void ResetCanvasScroll(void);
void GetCanvasViewportOrigin(int* pX, int* pY);
int Canvas_GetWidth(void);
void Canvas_SetWidth(int w);
int Canvas_GetHeight(void);
void Canvas_SetHeight(int h);
int Canvas_GetScrollX(void);
void Canvas_SetScrollX(int x);
int* Canvas_GetScrollXPtr(void);
int Canvas_GetScrollY(void);
void Canvas_SetScrollY(int y);
int* Canvas_GetScrollYPtr(void);
double Canvas_GetZoom(void);
void Canvas_SetZoom(double z);
void Canvas_ApplyZoomCentered(double newZoom);
#endif
