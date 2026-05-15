/*------------------------------------------------------------
   CONTROLLER.C — Application Logic Layer

   Handles mouse events, resize state machine, scroll/zoom,
   keyboard shortcuts, cursor management — everything that
   CANVASWNDPROC used to inline.
------------------------------------------------------------*/

#include "peztold_core.h"
#include "controller.h"
#include "canvas.h"
#include "cursors.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "tools.h"
#include "tools/bezier_tool.h"
#include "tools/crayon_tool.h"
#include "tools/freehand_tools.h"
#include "tools/highlighter_tool.h"
#include "tools/pen_tool.h"
#include "tools/polygon_tool.h"
#include "tools/shape_tools.h"
#include "tools/text_tool.h"
#include "ui/widgets/statusbar.h"

#include <math.h>

/*------------------------------------------------------------
   Resize State (Commit Pattern)
------------------------------------------------------------*/

#define HANDLE_SIZE 6
#define HANDLE_EDGE_OFFSET 2

typedef struct {
    BOOL active;
    ResizeHandleType handle;
    int startX, startY;
    int origW, origH;
    int newW, newH;
} ResizeState;

static ResizeState s_resize = {0};

static ResizeHandleType HitTestResizeHandle(HWND hwnd, int x, int y);
static void HandleScroll(HWND hwnd, int nBar, int nScrollCode);

static int s_lastToolX = -1;
static int s_lastToolY = -1;

/*------------------------------------------------------------
   Public: Controller_Init
------------------------------------------------------------*/

void Controller_Init(void) {
    s_resize.active = FALSE;
}

/*------------------------------------------------------------
   Public: Controller_HandleSize
------------------------------------------------------------*/

void Controller_HandleSize(HWND hwnd) {
    Controller_UpdateScrollbars(hwnd);
}

/*------------------------------------------------------------
   Public: Controller_UpdateScrollbars
------------------------------------------------------------*/

void Controller_UpdateScrollbars(HWND hwnd) {
    RECT rcClient;
    SCROLLINFO si;
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

/*------------------------------------------------------------
   Scroll Handler
------------------------------------------------------------*/

static void HandleScroll(HWND hwnd, int nBar, int nScrollCode) {
    SCROLLINFO si;
    int nPos;
    int *pScrollPos = (nBar == SB_HORZ) ? Canvas_GetScrollXPtr() : Canvas_GetScrollYPtr();

    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, nBar, &si);

    nPos = si.nPos;

    switch (nScrollCode) {
    case SB_LINEUP:    nPos -= 10; break;
    case SB_LINEDOWN:  nPos += 10; break;
    case SB_PAGEUP:    nPos -= si.nPage; break;
    case SB_PAGEDOWN:  nPos += si.nPage; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: nPos = si.nTrackPos; break;
    case SB_TOP:       nPos = si.nMin; break;
    case SB_BOTTOM:    nPos = si.nMax; break;
    }

    nPos = max(si.nMin, min(nPos, (int)(si.nMax - (si.nPage - 1))));

    if (nPos != *pScrollPos) {
        *pScrollPos = nPos;
        si.fMask = SIF_POS;
        si.nPos = nPos;
        SetScrollInfo(hwnd, nBar, &si, TRUE);
        ToolHandleLifecycleEvent(TOOL_LIFECYCLE_VIEWPORT_CHANGED, hwnd);
        InvalidateWindow(hwnd);
    }
}

/*------------------------------------------------------------
   Resize Handle Hit Test
------------------------------------------------------------*/

static ResizeHandleType HitTestResizeHandle(HWND hwnd, int xScreen,
                                             int yScreen) {
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

/*------------------------------------------------------------
   Coordinate Conversion Helper
------------------------------------------------------------*/

static BOOL BmpCoordInBounds(int xScr, int yScr, int *xBmp, int *yBmp) {
    CoordScrToBmp(xScr, yScr, xBmp, yBmp);
    if (*xBmp < 0 || *xBmp >= Canvas_GetWidth() ||
        *yBmp < 0 || *yBmp >= Canvas_GetHeight())
        return FALSE;
    return TRUE;
}

/*------------------------------------------------------------
   Public: Controller_IsResizing / Controller_GetResizePreview
------------------------------------------------------------*/

BOOL Controller_IsResizing(void) {
    return s_resize.active;
}

void Controller_GetResizePreview(int *outW, int *outH) {
    if (outW) *outW = s_resize.newW;
    if (outH) *outH = s_resize.newH;
}

/*------------------------------------------------------------
   Mouse Down Handler
------------------------------------------------------------*/

void Controller_HandleMouseDown(HWND hwnd, int screenX, int screenY, int btn) {
    ResizeHandleType handle = HitTestResizeHandle(hwnd, screenX, screenY);

    if (handle != RESIZE_HANDLE_NONE) {
        if (IsSelectionActive())
            CommitSelection();
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

    int xBitmap, yBitmap;
    if (!BmpCoordInBounds(screenX, screenY, &xBitmap, &yBitmap))
        return;

    s_lastToolX = screenX;
    s_lastToolY = screenY;
    ToolHandlePointerEvent(TOOL_POINTER_DOWN, hwnd, xBitmap, yBitmap, btn);
}

/*------------------------------------------------------------
   Double-Click Handler
------------------------------------------------------------*/

void Controller_HandleDoubleClick(HWND hwnd, int screenX, int screenY, int btn) {
    int xBitmap, yBitmap;
    if (!BmpCoordInBounds(screenX, screenY, &xBitmap, &yBitmap))
        return;
    ToolHandlePointerEvent(TOOL_POINTER_DOUBLE_CLICK, hwnd, xBitmap, yBitmap, btn);
}

/*------------------------------------------------------------
   Mouse Move Handler
------------------------------------------------------------*/

void Controller_HandleMouseMove(HWND hwnd, int screenX, int screenY, int wParam) {
    /* --- Resize dragging --- */
    if (s_resize.active) {
        int deltaX = screenX - s_resize.startX;
        int deltaY = screenY - s_resize.startY;

        int deltaW, deltaH;
        ScreenDeltaToBitmap(deltaX, deltaY, &deltaW, &deltaH);

        switch (s_resize.handle) {
        case RESIZE_HANDLE_RIGHT:
            s_resize.newW = max(1, s_resize.origW + deltaW);
            s_resize.newH = s_resize.origH;
            break;
        case RESIZE_HANDLE_BOTTOM:
            s_resize.newW = s_resize.origW;
            s_resize.newH = max(1, s_resize.origH + deltaH);
            break;
        case RESIZE_HANDLE_CORNER:
            s_resize.newW = max(1, s_resize.origW + deltaW);
            s_resize.newH = max(1, s_resize.origH + deltaH);
            break;
        default:
            break;
        }

        InvalidateWindow(hwnd);
        return;
    }

    /* --- Tool mouse move with Screen-Space Interpolation --- */
    if (s_lastToolX == -1 || !(wParam & (MK_LBUTTON | MK_RBUTTON))) {
        s_lastToolX = screenX;
        s_lastToolY = screenY;
    }

    int dx = screenX - s_lastToolX;
    int dy = screenY - s_lastToolY;
    int steps = max(abs(dx), abs(dy));

    // If mouse didn't move to a new screen pixel, still process the current point once
    if (steps == 0) steps = 1;

    for (int i = 1; i <= steps; i++) {
        int xCur = s_lastToolX + (dx * i) / steps;
        int yCur = s_lastToolY + (dy * i) / steps;

        int xBitmap, yBitmap;
        CoordScrToBmp(xCur, yCur, &xBitmap, &yBitmap);

        // Update status bar only for the final destination point
        if (i == steps) {
            int xClamp = max(-1000, min(xBitmap, 10000));
            int yClamp = max(-1000, min(yBitmap, 10000));
            StatusBarSetCoordinates(xClamp, yClamp);

            if (xBitmap >= 0 && xBitmap < Canvas_GetWidth() &&
                yBitmap >= 0 && yBitmap < Canvas_GetHeight()) {
                COLORREF c = LayersSampleCompositeColor(xBitmap, yBitmap, Palette_GetSecondaryColor());
                StatusBarSetColor(c);
            }
        }

        // Clamp bitmap coordinates to canvas bounds for tool safety
        if (xBitmap < 0) xBitmap = 0;
        if (xBitmap >= Canvas_GetWidth()) xBitmap = Canvas_GetWidth() - 1;
        if (yBitmap < 0) yBitmap = 0;
        if (yBitmap >= Canvas_GetHeight()) yBitmap = Canvas_GetHeight() - 1;

        ToolHandlePointerEvent(TOOL_POINTER_MOVE, hwnd, xBitmap, yBitmap, wParam);
    }

    s_lastToolX = screenX;
    s_lastToolY = screenY;
}

/*------------------------------------------------------------
   Mouse Up Handler
------------------------------------------------------------*/

void Controller_HandleMouseUp(HWND hwnd, int screenX, int screenY, int btn) {
    /* --- Resize commit / abort --- */
    if (s_resize.active) {
        ReleaseCapture();
        if (s_resize.newW != Canvas_GetWidth() || s_resize.newH != Canvas_GetHeight()) {
            char desc[64];
            StringCchPrintf(desc, sizeof(desc),
                "Resize Canvas: %dx%d -> %dx%d",
                Canvas_GetWidth(), Canvas_GetHeight(),
                s_resize.newW, s_resize.newH);
            if (ResizeCanvas(s_resize.newW, s_resize.newH)) {
                if (!HistoryPush(desc)) {
      /* Change applied, but undo entry could not be recorded. */
    }
                Controller_UpdateScrollbars(hwnd);
                InvalidateWindow(hwnd);
                SetDocumentDirty();
                Core_Notify(EV_LAYER_CONFIG);
            }
        }
        s_resize.active = FALSE;
        return;
    }

    /* --- Tool mouse up --- */
    int xBitmap, yBitmap;
    CoordScrToBmp(screenX, screenY, &xBitmap, &yBitmap);

    if (xBitmap < 0) xBitmap = 0;
    if (xBitmap >= Canvas_GetWidth()) xBitmap = Canvas_GetWidth() - 1;
    if (yBitmap < 0) yBitmap = 0;
    if (yBitmap >= Canvas_GetHeight()) yBitmap = Canvas_GetHeight() - 1;

    s_lastToolX = -1;
    s_lastToolY = -1;
    ToolHandlePointerEvent(TOOL_POINTER_UP, hwnd, xBitmap, yBitmap, btn);
}

/*------------------------------------------------------------
   Capture Lost Handler
------------------------------------------------------------*/

void Controller_HandleCaptureLost(HWND hwnd) {
    if (s_resize.active) {
        s_resize.active = FALSE;
    }
    s_lastToolX = -1;
    s_lastToolY = -1;
    ToolHandleLifecycleEvent(TOOL_LIFECYCLE_CAPTURE_LOST, hwnd);
}

/*------------------------------------------------------------
   Key Handler
------------------------------------------------------------*/

void Controller_HandleKey(HWND hwnd, WPARAM wParam, BOOL down) {
    /* Modifier keys: update cursor on both down and up */
    if (wParam == VK_MENU || wParam == VK_CONTROL || wParam == VK_SHIFT) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        SendMessage(hwnd, WM_SETCURSOR, (WPARAM)hwnd, MAKELPARAM(HTCLIENT, 0));
        return;
    }

    if (!down)
        return;

    /* Escape: cancel operations */
    if (wParam == VK_ESCAPE) {
        KillTimer(hwnd, TIMER_AIRBRUSH);
        if (s_resize.active) {
            s_resize.active = FALSE;
            ReleaseCapture();
            InvalidateWindow(hwnd);
            return;
        }
        ToolCancel();
        return;
    }

    /* Arrow keys: nudge selection (keydown only — avoids duplicate nudge on key-up) */
    if (Controller_IsResizing())
        return;
    if (IsSelectionActive() && !SelectionIsDragging() && !IsFreehandDrawing() &&
        !IsPolygonPending() && !IsShapePending() && !IsTextEditing() &&
        !IsPenDrawing() && !IsCurvePending() && !IsHighlighterDrawing() &&
        !IsCrayonDrawing()) {
        int dx = 0, dy = 0;
        if (wParam == VK_LEFT) dx = -1;
        else if (wParam == VK_RIGHT) dx = 1;
        else if (wParam == VK_UP) dy = -1;
        else if (wParam == VK_DOWN) dy = 1;

        if (dx != 0 || dy != 0) {
            if (IsCtrlDown()) { dx *= 10; dy *= 10; }
            SelectionMove(dx, dy);
            return;
        }
    }
}

/*------------------------------------------------------------
   Timer Handler
------------------------------------------------------------*/

void Controller_HandleTimer(HWND hwnd, WPARAM id) {
    if (id == TIMER_AIRBRUSH) {
        ToolHandleLifecycleEvent(TOOL_LIFECYCLE_TIMER_TICK, hwnd);
    }
}

/*------------------------------------------------------------
   Mouse Wheel Handler
------------------------------------------------------------*/

void Controller_HandleMouseWheel(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    /* CTRL+scroll => zoom */
    if (GetKeyState(VK_CONTROL) & 0x8000) {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        double zoomFactor = (delta > 0) ? 1.1 : (1.0 / 1.1);
        double newZoom = Canvas_GetZoom() * zoomFactor;

        if (newZoom < MIN_ZOOM_PERCENT)  newZoom = MIN_ZOOM_PERCENT;
        if (newZoom > MAX_ZOOM_PERCENT)  newZoom = MAX_ZOOM_PERCENT;

        if (newZoom != Canvas_GetZoom()) {
            POINT ptMouse;
            GetCursorPos(&ptMouse);
            ScreenToClient(hwnd, &ptMouse);

            double oldScale = Canvas_GetZoom() / 100.0;
            double newScale = newZoom / 100.0;

            int nDestX, nDestY;
            GetCanvasViewportOrigin(&nDestX, &nDestY);

            double xBmp = (double)(ptMouse.x + 0.5 - nDestX) / oldScale;
            double yBmp = (double)(ptMouse.y + 0.5 - nDestY) / oldScale;

            Canvas_SetZoom(newZoom);
            Controller_UpdateScrollbars(hwnd);
            GetCanvasViewportOrigin(&nDestX, &nDestY);

            int newXScreen = (int)floor(xBmp * newScale - 0.5) + nDestX;
            int newYScreen = (int)floor(yBmp * newScale - 0.5) + nDestY;

            Canvas_SetScrollX(Canvas_GetScrollX() + (newXScreen - ptMouse.x));
            Canvas_SetScrollY(Canvas_GetScrollY() + (newYScreen - ptMouse.y));

            SCROLLINFO si;
            si.cbSize = sizeof(si);
            si.fMask = SIF_RANGE | SIF_PAGE;
            GetScrollInfo(hwnd, SB_HORZ, &si);
            Canvas_SetScrollX(max(0, min(Canvas_GetScrollX(), (int)(si.nMax - (int)si.nPage + 1))));

            GetScrollInfo(hwnd, SB_VERT, &si);
            Canvas_SetScrollY(max(0, min(Canvas_GetScrollY(), (int)(si.nMax - (int)si.nPage + 1))));

            si.fMask = SIF_POS;
            si.nPos = Canvas_GetScrollX();
            SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
            si.nPos = Canvas_GetScrollY();
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

            SendMessage(GetParent(hwnd), WM_SIZE, 0, 0);
            ToolHandleLifecycleEvent(TOOL_LIFECYCLE_VIEWPORT_CHANGED, hwnd);
            InvalidateWindow(hwnd);
        }
        return;
    }

    /* Normal scroll */
    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    int nScrollCode = (delta > 0) ? SB_LINEUP : SB_LINEDOWN;
    int nBar = (GetKeyState(VK_SHIFT) & 0x8000) ? SB_HORZ : SB_VERT;

    for (int i = 0; i < 3; i++) {
        HandleScroll(hwnd, nBar, nScrollCode);
    }
}

/*------------------------------------------------------------
   Cursor Handler
------------------------------------------------------------*/

void Controller_HandleSetCursor(HWND hwnd, int screenX, int screenY) {
    ResizeHandleType handle = HitTestResizeHandle(hwnd, screenX, screenY);
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

    if (screenX < nCanvasLeft || screenX >= nCanvasLeft + nScaledW ||
        screenY < nCanvasTop || screenY >= nCanvasTop + nScaledH) {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return;
    }

    int xBitmap, yBitmap;
    CoordScrToBmp(screenX, screenY, &xBitmap, &yBitmap);
    SetToolCursor(Tool_GetCurrent(), xBitmap, yBitmap);
}

/*------------------------------------------------------------
   Scroll Message Handler (WM_HSCROLL / WM_VSCROLL)
------------------------------------------------------------*/

void Controller_HandleScroll(HWND hwnd, int nBar, int nScrollCode) {
    HandleScroll(hwnd, nBar, nScrollCode);
}
