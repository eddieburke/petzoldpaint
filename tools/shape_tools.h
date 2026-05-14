#ifndef SHAPE_TOOLS_H
#define SHAPE_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Line Tool
 *----------------------------------------------------------------------------*/

void LineToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void LineToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void LineToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Rectangle Tool
 *----------------------------------------------------------------------------*/

void RectToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void RectToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void RectToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Ellipse Tool
 *----------------------------------------------------------------------------*/

void EllipseToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void EllipseToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void EllipseToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Rounded Rectangle Tool
 *----------------------------------------------------------------------------*/

void RoundRectToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void RoundRectToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void RoundRectToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

/*------------------------------------------------------------------------------
 * Shared State / Lifecycle
 *----------------------------------------------------------------------------*/

BOOL IsShapeDrawing(void);
BOOL IsShapePending(void);
BOOL ShapeTool_IsBusy(void);
void CancelShapeDrawing(void);
void CommitPendingShape(void);

/* Tool VTable lifecycle hooks */
void ShapeTool_Deactivate(void);
BOOL ShapeTool_Cancel(void);

/*------------------------------------------------------------------------------
 * Overlay Drawing (screen-space handles)
 *----------------------------------------------------------------------------*/

void ShapeToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

#endif
