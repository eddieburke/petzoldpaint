#ifndef SHAPE_TOOLS_H
#define SHAPE_TOOLS_H

#include "../peztold_core.h"

typedef struct ShapeToolSnapshot {
  int state;
  int activeToolId;
  POINT ptStart;
  POINT ptEnd;
  int drawButton;
} ShapeToolSnapshot;

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
void ShapeTool_CommitPending(void);
ShapeToolSnapshot *ShapeTool_CreateSnapshot(void);
void ShapeTool_DestroySnapshot(ShapeToolSnapshot *snapshot);
void ShapeTool_ApplySnapshot(const ShapeToolSnapshot *snapshot);
/* Tool VTable lifecycle hooks */
void ShapeTool_Deactivate(void);
BOOL ShapeTool_Cancel(void);

/*------------------------------------------------------------------------------
 * Overlay Drawing (screen-space handles)
 *----------------------------------------------------------------------------*/

void ShapeToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

#endif
