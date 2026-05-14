#ifndef SHAPE_TOOLS_H
#define SHAPE_TOOLS_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Line Tool
 *----------------------------------------------------------------------------*/

void ShapeTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId);
void ShapeTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId);
void ShapeTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId);

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
