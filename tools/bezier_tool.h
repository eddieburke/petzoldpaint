#ifndef BEZIER_TOOL_H
#define BEZIER_TOOL_H

#include "../peztold_core.h"

#include <windows.h>

/* Mouse Event Handlers */
void BezierToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void BezierToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void BezierToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/* Overlay (screen-space control handles) */
void BezierToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

/* State / Lifecycle */
BOOL IsCurvePending(void);
void CommitPendingCurve(void);
BOOL BezierTool_Cancel(void);
void BezierTool_Deactivate(void);

#endif
