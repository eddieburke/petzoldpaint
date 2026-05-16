#include "shape_tools.h"
#include "canvas.h"
#include "draw.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "overlay.h"
#include "commit_bar.h"
#include "tools.h"
#include "tool_options/tool_options.h"
#include "palette.h"
#include "interaction.h"
#include <stdlib.h>
typedef enum { SHAPE_STATE_IDLE = 0, SHAPE_STATE_CREATING, SHAPE_STATE_EDITING, SHAPE_STATE_RESIZING } ShapeState;
#define ROUNDRECT_RADIUS 16
static struct {
	ShapeState state;
	int activeToolId;
	POINT
	ptStart;
	POINT
	ptEnd;
	int drawButton;
	int nHandle;
	POINT
	ptDragStart;
	POINT
	ptStartOrig;
	POINT
	ptEndOrig;
} s_Shape = {SHAPE_STATE_IDLE, 0, {0, 0}, {0, 0}, 0};
static void SnapEndpointIfShift(int *x, int *y) {
	if (!IsShiftDown())
		return;
	if (s_Shape.activeToolId == TOOL_LINE)
		SnapToAngle(s_Shape.ptStart.x, s_Shape.ptStart.y, x, y, 45);
	else
		SnapToSquare(s_Shape.ptStart.x, s_Shape.ptStart.y, x, y);
}
static void DrawShapeToBits(BYTE *bits, int width, int height) {
	COLORREF fg = GetColorForButton(s_Shape.drawButton);
	BYTE fgA = GetOpacityForButton(s_Shape.drawButton);
	COLORREF bg = (s_Shape.drawButton == MK_RBUTTON) ? Palette_GetPrimaryColor() : Palette_GetSecondaryColor();
	BYTE bgA = (s_Shape.drawButton == MK_RBUTTON) ? Palette_GetPrimaryOpacity() : Palette_GetSecondaryOpacity();
	int thick = nBrushWidth > 0 ? nBrushWidth : 1;
	if (s_Shape.activeToolId == TOOL_LINE) {
		DrawLineAlpha(bits, width, height, s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y, thick, fg, fgA, LAYER_BLEND_NORMAL);
		return;
	}
	RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y);
	int rw = rc.right - rc.left, rh = rc.bottom - rc.top;
	BOOL drawFill = (nShapeDrawType == 1 || nShapeDrawType == 2);
	BOOL drawOutline = (nShapeDrawType == 0 || nShapeDrawType == 2);
	COLORREF fillCol = (nShapeDrawType == 1) ? fg : bg;
	BYTE fillA = (nShapeDrawType == 1) ? fgA : bgA;
	switch (s_Shape.activeToolId) {
	case TOOL_RECT:
		if (drawFill)
			DrawRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, fillCol, fillA, LAYER_BLEND_NORMAL);
		if (drawOutline)
			DrawRectOutlineAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg, fgA, thick, LAYER_BLEND_NORMAL);
		break;
	case TOOL_ELLIPSE:
		if (drawFill)
			DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fillCol, fillA, TRUE, 0, LAYER_BLEND_NORMAL);
		if (drawOutline)
			DrawEllipseAlpha(bits, width, height, rc.left, rc.top, rw, rh, fg, fgA, FALSE, thick, LAYER_BLEND_NORMAL);
		break;
	case TOOL_ROUNDRECT:
		if (drawFill)
			DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, ROUNDRECT_RADIUS, fillCol, fillA, TRUE, 0, LAYER_BLEND_NORMAL);
		if (drawOutline)
			DrawRoundedRectAlpha(bits, width, height, rc.left, rc.top, rw, rh, ROUNDRECT_RADIUS, fg, fgA, FALSE, thick, LAYER_BLEND_NORMAL);
		break;
	}
}
static void UpdateDraftLayer(void) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	LayersClearDraft();
	BYTE *bits = LayersGetDraftBits();
	if (!bits)
		return;
	DrawShapeToBits(bits, Canvas_GetWidth(), Canvas_GetHeight());
	LayersMarkDirty();
}
static void ResetShapeState(void) {
	s_Shape.state = SHAPE_STATE_IDLE;
	s_Shape.activeToolId = 0;
	s_Shape.nHandle = HT_NONE;
}
BOOL IsShapePending(void) {
	return s_Shape.state != SHAPE_STATE_IDLE;
}
void ShapeTool_Deactivate(void) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	LayersClearDraft();
	Interaction_Abort();
	ResetShapeState();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
BOOL ShapeTool_Cancel(void) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return FALSE;
	ShapeTool_Deactivate();
	return TRUE;
}
void ShapeTool_CommitPending(void) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	UpdateDraftLayer();
	Interaction_MarkModified();
	Interaction_Commit("Draw Shape");
	ResetShapeState();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
BOOL ShapeTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton) {
	(void)hWnd;
	if (s_Shape.state != SHAPE_STATE_EDITING || nButton != MK_LBUTTON)
		return FALSE;
	RECT rcBounds = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y);
	int hit = CommitBar_HitTestScreen(&rcBounds, screenX, screenY);
	if (hit == COMMIT_BAR_HIT_COMMIT) {
		ShapeTool_CommitPending();
		return TRUE;
	}
	if (hit == COMMIT_BAR_HIT_CANCEL) {
		ShapeTool_Cancel();
		return TRUE;
	}
	return FALSE;
}
static void ShapeTool_HandleMouseDown(HWND hWnd, int x, int y, int nButton) {
	int toolId = Tool_GetCurrent();
	if (nButton == MK_RBUTTON && s_Shape.state != SHAPE_STATE_IDLE) {
		ShapeTool_Cancel();
		return;
	}
	if (s_Shape.state == SHAPE_STATE_IDLE) {
		LayersClearDraft();
		s_Shape.state = SHAPE_STATE_CREATING;
		s_Shape.activeToolId = toolId;
		s_Shape.drawButton = nButton;
		s_Shape.ptStart.x = x;
		s_Shape.ptStart.y = y;
		s_Shape.ptEnd = s_Shape.ptStart;
		if (!Interaction_Begin(hWnd, x, y, nButton, toolId)) {
			ResetShapeState();
			return;
		}
		return;
	}
	if (s_Shape.activeToolId != toolId)
		return;
	if (s_Shape.state == SHAPE_STATE_EDITING) {
		RECT rcBounds = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y);
		COMMIT_BAR_HANDLE_CLICK(&rcBounds, x, y, ShapeTool_CommitPending(), ShapeTool_Cancel());
		int hit = Overlay_HitTestBoxHandles(&rcBounds, x, y);
		if (hit != HT_NONE) {
			s_Shape.state = SHAPE_STATE_RESIZING;
			s_Shape.nHandle = hit;
			s_Shape.ptDragStart.x = x;
			s_Shape.ptDragStart.y = y;
			s_Shape.ptStartOrig = s_Shape.ptStart;
			s_Shape.ptEndOrig = s_Shape.ptEnd;
			SetCapture(hWnd);
			return;
		}
		ShapeTool_CommitPending();
		return;
	}
}
static void ShapeTool_HandleMouseMove(HWND hWnd, int x, int y, int nButton) {
	(void)hWnd;
	(void)nButton;
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	if (s_Shape.state == SHAPE_STATE_CREATING) {
		int endX = x, endY = y;
		SnapEndpointIfShift(&endX, &endY);
		if (s_Shape.ptEnd.x != endX || s_Shape.ptEnd.y != endY) {
			s_Shape.ptEnd.x = endX;
			s_Shape.ptEnd.y = endY;
			UpdateDraftLayer();
			Interaction_MarkModified();
			InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		}
		return;
	}
	if (s_Shape.state == SHAPE_STATE_RESIZING && GetCapture() == hWnd) {
		int dx = x - s_Shape.ptDragStart.x;
		int dy = y - s_Shape.ptDragStart.y;
		RECT rc = GetRectFromPoints(s_Shape.ptStartOrig.x, s_Shape.ptStartOrig.y, s_Shape.ptEndOrig.x, s_Shape.ptEndOrig.y);
		if (s_Shape.nHandle == HT_BODY) {
			OffsetRect(&rc, dx, dy);
		} else {
			ResizeRect(&rc, s_Shape.nHandle, dx, dy, 2, IsShiftDown());
			NormalizeRect(&rc);
		}
		s_Shape.ptStart.x = rc.left;
		s_Shape.ptStart.y = rc.top;
		s_Shape.ptEnd.x = rc.right;
		s_Shape.ptEnd.y = rc.bottom;
		UpdateDraftLayer();
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
static void ShapeTool_HandleMouseUp(HWND hWnd, int x, int y, int nButton) {
	(void)hWnd;
	(void)nButton;
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	if (s_Shape.state == SHAPE_STATE_CREATING) {
		int endX = x, endY = y;
		SnapEndpointIfShift(&endX, &endY);
		s_Shape.ptEnd.x = endX;
		s_Shape.ptEnd.y = endY;
		UpdateDraftLayer();
		Interaction_MarkModified();
		s_Shape.state = SHAPE_STATE_EDITING;
		HistoryPushSession("Create Shape");
		return;
	}
	if (s_Shape.state == SHAPE_STATE_RESIZING) {
		if (GetCapture() == hWnd)
			ReleaseCapture();
		s_Shape.state = SHAPE_STATE_EDITING;
		s_Shape.nHandle = HT_NONE;
		UpdateDraftLayer();
		HistoryPushSession("Adjust Shape");
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
void ShapeTool_OnPointer(const ToolPointerEvent *ev) {
	if (!ev)
		return;
	switch (ev->type) {
	case TOOL_POINTER_DOWN:
		ShapeTool_HandleMouseDown(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_MOVE:
		ShapeTool_HandleMouseMove(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_UP:
		ShapeTool_HandleMouseUp(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	default:
		break;
	}
}
void ShapeTool_DrawOverlay(const OverlayContext *ctx) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return;
	if (s_Shape.activeToolId == TOOL_LINE) {
		POINT pts[2] = {s_Shape.ptStart, s_Shape.ptEnd};
		Overlay_DrawPolyHandles(ctx, pts, 2);
	} else {
		RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y);
		Overlay_DrawBoxHandles(ctx, &rc);
	}
	if (s_Shape.state == SHAPE_STATE_EDITING) {
		RECT rc = GetRectFromPoints(s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.ptEnd.x, s_Shape.ptEnd.y);
		CommitBar_Draw(ctx, &rc);
	}
}
ShapeToolSnapshot *ShapeTool_CreateSnapshot(void) {
	if (s_Shape.state == SHAPE_STATE_IDLE)
		return NULL;
	ShapeToolSnapshot *snapshot = (ShapeToolSnapshot *)calloc(1, sizeof(ShapeToolSnapshot));
	if (!snapshot)
		return NULL;
	snapshot->state = s_Shape.state;
	snapshot->activeToolId = s_Shape.activeToolId;
	snapshot->ptStart = s_Shape.ptStart;
	snapshot->ptEnd = s_Shape.ptEnd;
	snapshot->drawButton = s_Shape.drawButton;
	return snapshot;
}
void ShapeTool_DestroySnapshot(ShapeToolSnapshot *snapshot) {
	free(snapshot);
}
void ShapeTool_ApplySnapshot(const ShapeToolSnapshot *snapshot) {
	if (s_Shape.state != SHAPE_STATE_IDLE) {
		LayersClearDraft();
		if (Interaction_IsActive())
			Interaction_EndQuiet();
	}
	ResetShapeState();
	if (!snapshot) {
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return;
	}
	s_Shape.state = (ShapeState)snapshot->state;
	s_Shape.activeToolId = snapshot->activeToolId;
	s_Shape.ptStart = snapshot->ptStart;
	s_Shape.ptEnd = snapshot->ptEnd;
	s_Shape.drawButton = snapshot->drawButton;
	s_Shape.nHandle = HT_NONE;
	if (!Interaction_IsActive()) {
		(void)Interaction_BeginEx(GetCanvasWindow(), s_Shape.ptStart.x, s_Shape.ptStart.y, s_Shape.drawButton, s_Shape.activeToolId, FALSE);
	}
	UpdateDraftLayer();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
