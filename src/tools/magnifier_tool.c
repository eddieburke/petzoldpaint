#include "magnifier_tool.h"
#include "peztold_core.h"
#include "geom.h"
#include "helpers.h"
#include "gdi_utils.h"
#include "canvas.h"
static BOOL bMagnifierDragging = FALSE;
static int nMagnifierStartX = 0;
static int nMagnifierStartY = 0;
static int nMagnifierEndX = 0;
static int nMagnifierEndY = 0;
static void MagnifierTool_HandleMouseDown(HWND hWnd, int x, int y, int nButton) {
	if (nButton == MK_LBUTTON) {
		bMagnifierDragging = TRUE;
		nMagnifierStartX = x;
		nMagnifierStartY = y;
		nMagnifierEndX = x;
		nMagnifierEndY = y;
		SetCapture(hWnd);
	} else {
		Canvas_SetZoom(100.0);
		SendMessage(GetParent(hWnd), WM_SIZE, 0, 0);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
static void MagnifierTool_HandleMouseMove(HWND hWnd, int x, int y, int nButton) {
	(void)hWnd;
	(void)nButton;
	if (bMagnifierDragging) {
		nMagnifierEndX = x;
		nMagnifierEndY = y;
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
static void MagnifierTool_HandleMouseUp(HWND hWnd, int x, int y, int nButton) {
	(void)x;
	(void)y;
	(void)nButton;
	if (!bMagnifierDragging)
		return;
	ReleaseCapture();
	bMagnifierDragging = FALSE;
	int left = min(nMagnifierStartX, nMagnifierEndX);
	int top = min(nMagnifierStartY, nMagnifierEndY);
	int right = max(nMagnifierStartX, nMagnifierEndX);
	int bottom = max(nMagnifierStartY, nMagnifierEndY);
	int selWBitmap = right - left;
	int selHBitmap = bottom - top;
	if (selWBitmap < 4 || selHBitmap < 4) {
		if (Canvas_GetZoom() < 800) {
			if (Canvas_GetZoom() < 100)
				Canvas_SetZoom(100);
			else
				Canvas_SetZoom(Canvas_GetZoom() * 2);
		}
	} else {
		RECT rcClient;
		GetClientRect(hWnd, &rcClient);
		int viewW = rcClient.right - rcClient.left;
		int viewH = rcClient.bottom - rcClient.top;
		double zoomW = (double)(viewW * 100.0) / selWBitmap;
		double zoomH = (double)(viewH * 100.0) / selHBitmap;
		Canvas_SetZoom(min(zoomW, zoomH));
		if (Canvas_GetZoom() < MIN_ZOOM_PERCENT)
			Canvas_SetZoom(MIN_ZOOM_PERCENT);
		if (Canvas_GetZoom() > MAX_ZOOM_PERCENT)
			Canvas_SetZoom(MAX_ZOOM_PERCENT);
	}
	SendMessage(GetParent(hWnd), WM_SIZE, 0, 0);
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
void MagnifierTool_OnPointer(const ToolPointerEvent *ev) {
	if (!ev)
		return;
	switch (ev->type) {
	case TOOL_POINTER_DOWN:
		MagnifierTool_HandleMouseDown(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_MOVE:
		MagnifierTool_HandleMouseMove(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_UP:
		MagnifierTool_HandleMouseUp(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	default:
		break;
	}
}
void MagnifierTool_DrawOverlay(const OverlayContext *ctx) {
	if (!bMagnifierDragging)
		return;
	HPEN hOldPen;
	HPEN hPen = CreatePenAndSelect(ctx->hdc, PS_DOT, 1, RGB(0, 0, 0), &hOldPen);
	HBRUSH hBrushOld = (HBRUSH)SelectObject(ctx->hdc, GetStockObject(NULL_BRUSH));
	int x1, y1, x2, y2;
	Viewport_BmpToScr(&ctx->vp, nMagnifierStartX, nMagnifierStartY, &x1, &y1);
	Viewport_BmpToScr(&ctx->vp, nMagnifierEndX, nMagnifierEndY, &x2, &y2);
	Rectangle(ctx->hdc, x1, y1, x2, y2);
	SelectObject(ctx->hdc, hBrushOld);
	RestorePen(ctx->hdc, hOldPen);
	Gdi_DeletePen(hPen);
}
void MagnifierTool_Deactivate(void) {
	if (bMagnifierDragging) {
		bMagnifierDragging = FALSE;
		ReleaseCapture();
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
void MagnifierTool_OnCaptureLost(void) {
	if (!bMagnifierDragging)
		return;
	bMagnifierDragging = FALSE;
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
