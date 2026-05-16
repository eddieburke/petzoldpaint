#ifndef SHAPE_TOOLS_H
#define SHAPE_TOOLS_H
#include "peztold_core.h"
#include "tools.h"
typedef struct ShapeToolSnapshot {
	int state;
	int activeToolId;
	POINT
	ptStart;
	POINT
	ptEnd;
	int drawButton;
} ShapeToolSnapshot;
void ShapeTool_OnPointer(const ToolPointerEvent *ev);
BOOL ShapeTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton);
BOOL IsShapePending(void);
void ShapeTool_CommitPending(void);
ShapeToolSnapshot *ShapeTool_CreateSnapshot(void);
void ShapeTool_DestroySnapshot(ShapeToolSnapshot *snapshot);
void ShapeTool_ApplySnapshot(const ShapeToolSnapshot *snapshot);
void ShapeTool_Deactivate(void);
BOOL ShapeTool_Cancel(void);
void ShapeTool_DrawOverlay(const OverlayContext *ctx);
#endif
