#include "peztold_core.h"
#include "controller.h"
#include "canvas.h"
#include "cursors.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "tools.h"
#include "interaction.h"
#include "tools/selection_tool.h"
#include "ui/widgets/statusbar.h"
#include <math.h>
#define HANDLE_SIZE        6
#define HANDLE_EDGE_OFFSET 2
typedef struct {
	BOOL active;
	ResizeHandleType handle;
	int startX, startY;
	int origW, origH;
	int newW, newH;
} ResizeState;
typedef struct {
	int screenX, screenY;
	int bmpX, bmpY;
	int clampedX, clampedY;
	UINT buttons;
	BOOL insideBitmap;
} ControllerPointer;
static ResizeState s_resize = {0};
static ResizeHandleType HitTestResizeHandle(int x, int y);
static int s_lastToolX = -1;
static int s_lastToolY = -1;
static void ClampBitmapCoord(int *x, int *y) {
	int w = Canvas_GetWidth(), h = Canvas_GetHeight();
	if (*x < 0)
		*x = 0;
	else if (*x >= w)
		*x = w - 1;
	if (*y < 0)
		*y = 0;
	else if (*y >= h)
		*y = h - 1;
}
static int ClampCanvasDimension(int value) {
	if (value < CANVAS_MIN_DIM)
		return CANVAS_MIN_DIM;
	if (value > CANVAS_MAX_DIM)
		return CANVAS_MAX_DIM;
	return value;
}
static ControllerPointer MakePointer(int screenX, int screenY, UINT buttons) {
	ControllerPointer pt;
	pt.screenX = screenX;
	pt.screenY = screenY;
	pt.buttons = buttons;
	CoordScrToBmp(screenX, screenY, &pt.bmpX, &pt.bmpY);
	pt.clampedX = pt.bmpX;
	pt.clampedY = pt.bmpY;
	pt.insideBitmap = pt.bmpX >= 0 && pt.bmpX < Canvas_GetWidth() && pt.bmpY >= 0 && pt.bmpY < Canvas_GetHeight();
	ClampBitmapCoord(&pt.clampedX, &pt.clampedY);
	return pt;
}
static void DispatchToolPointer(ToolPointerEventType type, HWND hwnd, const ControllerPointer *pt) {
	ToolPointerEvent ev = {type, hwnd, {pt->clampedX, pt->clampedY}, pt->buttons};
	ToolHandlePointerEvent(&ev);
}
void Controller_HandleSize(HWND hwnd) {
	Controller_UpdateScrollbars(hwnd);
}
void Controller_UpdateScrollbars(HWND hwnd) {
	RECT rcClient;
	SCROLLINFO
	si;
	int nScaledW, nScaledH;
	GetClientRect(hwnd, &rcClient);
	GetScaledDimensions(Canvas_GetWidth(), Canvas_GetHeight(), &nScaledW, &nScaledH);
	nScaledW += 4;
	nScaledH += 4;
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = nScaledW - 1;
	si.nPage = rcClient.right;
	si.nPos = Canvas_GetScrollX();
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
	GetScrollInfo(hwnd, SB_HORZ, &si);
	Canvas_SetScrollX(si.nPos);
	si.cbSize = sizeof(si);
	si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin = 0;
	si.nMax = nScaledH - 1;
	si.nPage = rcClient.bottom;
	si.nPos = Canvas_GetScrollY();
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	GetScrollInfo(hwnd, SB_VERT, &si);
	Canvas_SetScrollY(si.nPos);
}
static void HandleScrollEx(HWND hwnd, int nBar, int nScrollCode, int lineDelta) {
	SCROLLINFO
	si;
	int nPos;
	int *pScrollPos = (nBar == SB_HORZ) ? Canvas_GetScrollXPtr() : Canvas_GetScrollYPtr();
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	GetScrollInfo(hwnd, nBar, &si);
	nPos = si.nPos;
	switch (nScrollCode) {
	case SB_LINEUP:
		nPos -= lineDelta;
		break;
	case SB_LINEDOWN:
		nPos += lineDelta;
		break;
	case SB_PAGEUP:
		nPos -= si.nPage;
		break;
	case SB_PAGEDOWN:
		nPos += si.nPage;
		break;
	case SB_THUMBTRACK:
	case SB_THUMBPOSITION:
		nPos = si.nTrackPos;
		break;
	case SB_TOP:
		nPos = si.nMin;
		break;
	case SB_BOTTOM:
		nPos = si.nMax;
		break;
	}
	nPos = max(si.nMin, min(nPos, (int)(si.nMax - (si.nPage - 1))));
	if (nPos != *pScrollPos) {
		*pScrollPos = nPos;
		si.fMask = SIF_POS;
		si.nPos = nPos;
		SetScrollInfo(hwnd, nBar, &si, TRUE);
		ToolHandleLifecycleEvent(TOOL_LIFECYCLE_VIEWPORT_CHANGED, hwnd);
		InvalidateRect(hwnd, NULL, FALSE);
	}
}
static ResizeHandleType HitTestResizeHandle(int xScreen, int yScreen) {
	if (IsSelectionActive() || Tool_IsCurrentBusy())
		return RESIZE_HANDLE_NONE;
	int nScaledW, nScaledH;
	GetScaledDimensions(Canvas_GetWidth(), Canvas_GetHeight(), &nScaledW, &nScaledH);
	int nDestX, nDestY;
	GetCanvasViewportOrigin(&nDestX, &nDestY);
	RECT rcCorner, rcRight, rcBottom;
	rcCorner.left = nDestX + nScaledW + HANDLE_EDGE_OFFSET;
	rcCorner.top = nDestY + nScaledH + HANDLE_EDGE_OFFSET;
	rcCorner.right = rcCorner.left + HANDLE_SIZE;
	rcCorner.bottom = rcCorner.top + HANDLE_SIZE;
	rcRight.left = nDestX + nScaledW + HANDLE_EDGE_OFFSET;
	rcRight.top = nDestY + nScaledH / 2 - HANDLE_SIZE / 2;
	rcRight.right = rcRight.left + HANDLE_SIZE;
	rcRight.bottom = rcRight.top + HANDLE_SIZE;
	rcBottom.left = nDestX + nScaledW / 2 - HANDLE_SIZE / 2;
	rcBottom.top = nDestY + nScaledH + HANDLE_EDGE_OFFSET;
	rcBottom.right = rcBottom.left + HANDLE_SIZE;
	rcBottom.bottom = rcBottom.top + HANDLE_SIZE;
	POINT pt = {xScreen, yScreen};
	if (PtInRect(&rcCorner, pt))
		return RESIZE_HANDLE_CORNER;
	if (PtInRect(&rcRight, pt))
		return RESIZE_HANDLE_RIGHT;
	if (PtInRect(&rcBottom, pt))
		return RESIZE_HANDLE_BOTTOM;
	return RESIZE_HANDLE_NONE;
}
BOOL Controller_IsResizing(void) {
	return s_resize.active;
}
void Controller_GetResizePreview(int *outW, int *outH) {
	if (outW)
		*outW = s_resize.newW;
	if (outH)
		*outH = s_resize.newH;
}
void Controller_HandleMouseDown(HWND hwnd, int screenX, int screenY, int btn) {
	ControllerPointer pt = MakePointer(screenX, screenY, (UINT)btn);
	if (ToolHandleOverlayPointerEvent(hwnd, screenX, screenY, btn))
		return;
	ResizeHandleType handle = (btn == MK_LBUTTON) ? HitTestResizeHandle(screenX, screenY) : RESIZE_HANDLE_NONE;
	if (handle != RESIZE_HANDLE_NONE) {
		s_resize.active = TRUE;
		s_resize.handle = handle;
		s_resize.startX = screenX;
		s_resize.startY = screenY;
		s_resize.origW = Canvas_GetWidth();
		s_resize.origH = Canvas_GetHeight();
		s_resize.newW = Canvas_GetWidth();
		s_resize.newH = Canvas_GetHeight();
		SetCapture(hwnd);
		return;
	}
	if (!pt.insideBitmap)
		return;
	s_lastToolX = screenX;
	s_lastToolY = screenY;
	DispatchToolPointer(TOOL_POINTER_DOWN, hwnd, &pt);
}
void Controller_HandleDoubleClick(HWND hwnd, int screenX, int screenY, int btn) {
	ControllerPointer pt = MakePointer(screenX, screenY, (UINT)btn);
	if (!pt.insideBitmap)
		return;
	DispatchToolPointer(TOOL_POINTER_DOUBLE_CLICK, hwnd, &pt);
}
void Controller_HandleMouseMove(HWND hwnd, int screenX, int screenY, int wParam) {
	if (s_resize.active) {
		int deltaX = screenX - s_resize.startX;
		int deltaY = screenY - s_resize.startY;
		int deltaW, deltaH;
		ScreenDeltaToBitmap(deltaX, deltaY, &deltaW, &deltaH);
		switch (s_resize.handle) {
		case RESIZE_HANDLE_RIGHT:
			s_resize.newW = ClampCanvasDimension(s_resize.origW + deltaW);
			s_resize.newH = s_resize.origH;
			break;
		case RESIZE_HANDLE_BOTTOM:
			s_resize.newW = s_resize.origW;
			s_resize.newH = ClampCanvasDimension(s_resize.origH + deltaH);
			break;
		case RESIZE_HANDLE_CORNER:
			s_resize.newW = ClampCanvasDimension(s_resize.origW + deltaW);
			s_resize.newH = ClampCanvasDimension(s_resize.origH + deltaH);
			break;
		default:
			break;
		}
		InvalidateRect(hwnd, NULL, FALSE);
		return;
	}
	if (s_lastToolX == -1 || !(wParam & (MK_LBUTTON | MK_RBUTTON))) {
		s_lastToolX = screenX;
		s_lastToolY = screenY;
	}
	int dx = screenX - s_lastToolX;
	int dy = screenY - s_lastToolY;
	int steps = max(abs(dx), abs(dy));
	if (steps == 0) {
		steps = 1;
	} else if (steps > CONTROLLER_MAX_POINTER_SUBSTEPS) {
		steps = CONTROLLER_MAX_POINTER_SUBSTEPS;
	}
	for (int i = 1; i <= steps; i++) {
		int xCur = s_lastToolX + (dx * i) / steps;
		int yCur = s_lastToolY + (dy * i) / steps;
		ControllerPointer pt = MakePointer(xCur, yCur, (UINT)wParam);
		if (i == steps) {
			int xClamp = max(-1000, min(pt.bmpX, 10000));
			int yClamp = max(-1000, min(pt.bmpY, 10000));
			StatusBarSetCoordinates(xClamp, yClamp);
			if (!(wParam & (MK_LBUTTON | MK_RBUTTON)) && pt.insideBitmap) {
				COLORREF c = LayersSampleCompositeColor(pt.bmpX, pt.bmpY);
				StatusBarSetColor(c);
			}
		}
		DispatchToolPointer(TOOL_POINTER_MOVE, hwnd, &pt);
	}
	if ((wParam & (MK_LBUTTON | MK_RBUTTON)) && Interaction_IsActive())
		Interaction_FlushStrokeRedraw();
	s_lastToolX = screenX;
	s_lastToolY = screenY;
}
void Controller_HandleMouseUp(HWND hwnd, int screenX, int screenY, int btn) {
	if (s_resize.active) {
		ReleaseCapture();
		if (s_resize.newW != Canvas_GetWidth() || s_resize.newH != Canvas_GetHeight()) {
			char desc[64];
			StringCchPrintf(desc, sizeof(desc), "Resize Canvas: %dx%d -> %dx%d", Canvas_GetWidth(), Canvas_GetHeight(), s_resize.newW, s_resize.newH);
			if (ResizeCanvas(s_resize.newW, s_resize.newH)) {
				(void)HistoryPush(desc);
				Controller_UpdateScrollbars(hwnd);
				InvalidateRect(hwnd, NULL, FALSE);
				SetDocumentDirty();
				Core_Notify(EV_LAYER_CONFIG);
			}
		}
		s_resize.active = FALSE;
		return;
	}
	ControllerPointer pt = MakePointer(screenX, screenY, (UINT)btn);
	s_lastToolX = -1;
	s_lastToolY = -1;
	DispatchToolPointer(TOOL_POINTER_UP, hwnd, &pt);
}
void Controller_HandleCaptureLost(HWND hwnd) {
	if (s_resize.active) {
		s_resize.active = FALSE;
		InvalidateRect(hwnd, NULL, FALSE);
	}
	s_lastToolX = -1;
	s_lastToolY = -1;
	ToolHandleLifecycleEvent(TOOL_LIFECYCLE_CAPTURE_LOST, hwnd);
}
void Controller_HandleKey(HWND hwnd, WPARAM wParam, BOOL down) {
	if (wParam == VK_MENU || wParam == VK_CONTROL || wParam == VK_SHIFT) {
		POINT
		pt;
		GetCursorPos(&pt);
		ScreenToClient(hwnd, &pt);
		SendMessage(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, 0));
		return;
	}
	if (!down)
		return;
	if (wParam == VK_ESCAPE) {
		KillTimer(hwnd, TIMER_AIRBRUSH);
		if (s_resize.active) {
			s_resize.active = FALSE;
			ReleaseCapture();
			InvalidateRect(hwnd, NULL, FALSE);
			return;
		}
		ToolCancel(TOOL_CANCEL_ABORT, FALSE);
		return;
	}
	if (Controller_IsResizing())
		return;
	if (IsSelectionActive() && !SelectionIsDragging() && !Tool_IsCurrentBusy()) {
		int dx = 0, dy = 0;
		if (wParam == VK_LEFT)
			dx = -1;
		else if (wParam == VK_RIGHT)
			dx = 1;
		else if (wParam == VK_UP)
			dy = -1;
		else if (wParam == VK_DOWN)
			dy = 1;
		if (dx != 0 || dy != 0) {
			if (IsCtrlDown()) {
				dx *= 10;
				dy *= 10;
			}
			SelectionMove(dx, dy);
			return;
		}
	}
}
void Controller_HandleTimer(HWND hwnd, WPARAM id) {
	if (id == TIMER_AIRBRUSH) {
		ToolHandleLifecycleEvent(TOOL_LIFECYCLE_TIMER_TICK, hwnd);
	}
}
void Controller_HandleMouseWheel(HWND hwnd, WPARAM wParam, LPARAM lParam) {
	(void)lParam;
	if (GetKeyState(VK_CONTROL) & 0x8000) {
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		double zoomFactor = (delta > 0) ? 1.1 : (1.0 / 1.1);
		double newZoom = Canvas_GetZoom() * zoomFactor;
		if (newZoom < MIN_ZOOM_PERCENT)
			newZoom = MIN_ZOOM_PERCENT;
		if (newZoom > MAX_ZOOM_PERCENT)
			newZoom = MAX_ZOOM_PERCENT;
		if (newZoom != Canvas_GetZoom()) {
			POINT
			ptMouse;
			GetCursorPos(&ptMouse);
			ScreenToClient(hwnd, &ptMouse);
			Canvas_ZoomAroundPoint(newZoom, ptMouse.x, ptMouse.y);
		}
		return;
	}
	int delta = GET_WHEEL_DELTA_WPARAM(wParam);
	int nScrollCode = (delta > 0) ? SB_LINEUP : SB_LINEDOWN;
	int nBar = (GetKeyState(VK_SHIFT) & 0x8000) ? SB_HORZ : SB_VERT;
	HandleScrollEx(hwnd, nBar, nScrollCode, 30);
}
void Controller_HandleSetCursor(HWND hwnd, int screenX, int screenY) {
	ResizeHandleType handle = HitTestResizeHandle(screenX, screenY);
	if (handle != RESIZE_HANDLE_NONE) {
		switch (handle) {
		case RESIZE_HANDLE_RIGHT:
			SetCursor(LoadCursor(NULL, IDC_SIZEWE));
			break;
		case RESIZE_HANDLE_BOTTOM:
			SetCursor(LoadCursor(NULL, IDC_SIZENS));
			break;
		case RESIZE_HANDLE_CORNER:
			SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
			break;
		default:
			break;
		}
		return;
	}
	int nScaledW, nScaledH;
	GetScaledDimensions(Canvas_GetWidth(), Canvas_GetHeight(), &nScaledW, &nScaledH);
	int nCanvasLeft, nCanvasTop;
	GetCanvasViewportOrigin(&nCanvasLeft, &nCanvasTop);
	if (screenX < nCanvasLeft || screenX >= nCanvasLeft + nScaledW || screenY < nCanvasTop || screenY >= nCanvasTop + nScaledH) {
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return;
	}
	int xBitmap, yBitmap;
	CoordScrToBmp(screenX, screenY, &xBitmap, &yBitmap);
	SetToolCursor(Tool_GetCurrent(), xBitmap, yBitmap);
}
void Controller_HandleScroll(HWND hwnd, int nBar, int nScrollCode) {
	HandleScrollEx(hwnd, nBar, nScrollCode, 10);
}
