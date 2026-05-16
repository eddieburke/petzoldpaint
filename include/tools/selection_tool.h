#ifndef SELECTION_TOOL_H
#define SELECTION_TOOL_H
#include "peztold_core.h"
#include "helpers.h"
#include "pixel_ops.h"
#include "tools.h"
#include <windows.h>
void SelectionTool_OnPointer(const ToolPointerEvent *ev);
void SelectionTool_OnCaptureLost(void);
BOOL SelectionTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton);
void SelectionTool_DrawOverlay(const OverlayContext *ctx);
void SelectionTool_Deactivate(void);
BOOL SelectionTool_Cancel(ToolCancelReason reason);
void SelectionClearState(void);
void CommitSelection(void);
void CancelSelection(void);
BOOL IsSelectionActive(void);
BOOL SelectionIsDragging(void);
BOOL IsPointInSelection(int x, int y);
void SelectionCopy(void);
void SelectionCut(void);
void SelectionPaste(HWND hWnd);
void SelectionInvert(void);
void SelectionInvertColors(void);
void SelectionDelete(void);
void SelectionSelectAll(void);
void SelectionRotate(int degrees);
void SelectionFlip(BOOL bHorz);
int SelectionGetCursorId(int x, int y);
void SelectionMove(int dx, int dy);
#include "poly_store.h"
typedef struct SelectionSnapshot {
	int mode;
	RECT rcBounds;
	HRGN hRegion;
	HBITMAP
	hFloatBmp;
	BYTE *pFloatBits;
	int nFloatW, nFloatH;
	HBITMAP
	hBackupBmp;
	BYTE *pBackupBits;
	HRGN hBackupRegion;
	RECT rcLiftOrigin;
	double fRotationAngle;
	POINT
	ptRotateCenter;
	PolyStore freeformPts;
} SelectionSnapshot;
SelectionSnapshot *Selection_CreateSnapshot(void);
void Selection_DestroySnapshot(SelectionSnapshot *snapshot);
void Selection_ApplySnapshot(const SelectionSnapshot *snapshot);
#endif
