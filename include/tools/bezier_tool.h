#ifndef BEZIER_TOOL_H
#define BEZIER_TOOL_H
#include "peztold_core.h"
#include "tools.h"
#include <windows.h>
typedef struct BezierToolSnapshot {
	int state;
	POINT
	ptStart;
	POINT
	ptEnd;
	POINT
	ptCtrl1;
	POINT
	ptCtrl2;
	int drawButton;
} BezierToolSnapshot;
void BezierTool_OnPointer(const ToolPointerEvent *ev);
BOOL BezierTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton);
void BezierTool_DrawOverlay(const OverlayContext *ctx);
BOOL IsCurvePending(void);
BOOL BezierTool_Cancel(void);
void BezierTool_Deactivate(void);
void BezierTool_CommitPending(void);
BezierToolSnapshot *BezierTool_CreateSnapshot(void);
void BezierTool_DestroySnapshot(BezierToolSnapshot *snapshot);
void BezierTool_ApplySnapshot(const BezierToolSnapshot *snapshot);
#endif
