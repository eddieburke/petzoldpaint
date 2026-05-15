#ifndef SHAPE_TOOLS_H
#define SHAPE_TOOLS_H

#include "peztold_core.h"

typedef struct ShapeToolSnapshot {
  int state;
  int activeToolId;
  POINT ptStart;
  POINT ptEnd;
  int drawButton;
} ShapeToolSnapshot;


void ShapeTool_OnMouseDown(HWND hWnd, int x, int y, int nButton, int toolId);
void ShapeTool_OnMouseMove(HWND hWnd, int x, int y, int nButton, int toolId);
void ShapeTool_OnMouseUp(HWND hWnd, int x, int y, int nButton, int toolId);


BOOL IsShapePending(void);
BOOL ShapeTool_IsBusy(void);
void ShapeTool_CommitPending(void);
ShapeToolSnapshot *ShapeTool_CreateSnapshot(void);
void ShapeTool_DestroySnapshot(ShapeToolSnapshot *snapshot);
void ShapeTool_ApplySnapshot(const ShapeToolSnapshot *snapshot);
void ShapeTool_Deactivate(void);
BOOL ShapeTool_Cancel(void);


void ShapeTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

#endif
