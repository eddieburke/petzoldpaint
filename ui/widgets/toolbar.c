#include "toolbar.h"
#include "../../tools.h"
#include "../../tools/tool_options/tool_options.h"
#include "../../resource.h"
#include "../../tools/selection_tool.h"
#include "../../tools/text_tool.h"
#include "../../canvas.h"
#include "../../helpers.h"
#include "../../draw.h"
#include "../../gdi_utils.h"
#include <commctrl.h>
static HWND hToolbarWnd = NULL;
static HBITMAP hToolIcons = NULL;
static HBITMAP hToolIconsExtra = NULL;
static int nPressIndex = -1;
static int nHotIndex = -1;
#define ICON_WIDTH  16
#define ICON_HEIGHT 16
#define ICON_COLORKEY RGB(255, 0, 0)
#define TOOLBTN_SIZE 24
#define TOOLBTN_PADDING 2
#define TOOLBAR_COLS 2

static void GetToolRect(int index, RECT* rc) {
    int col = index % TOOLBAR_COLS;
    int row = index / TOOLBAR_COLS;
    int x = TOOLBTN_PADDING + col * (TOOLBTN_SIZE + TOOLBTN_PADDING);
    int y = TOOLBTN_PADDING + row * (TOOLBTN_SIZE + TOOLBTN_PADDING);
    rc->left = x;
    rc->top = y;
    rc->right = x + TOOLBTN_SIZE;
    rc->bottom = y + TOOLBTN_SIZE;
}

static int HitTestTool(int x, int y) {
    RECT rc;
    POINT pt;
    pt.x = x;
    pt.y = y;
    for (int i = 0; i < NUM_TOOLS; i++) {
        GetToolRect(i, &rc);
        if (PtInRect(&rc, pt)) return i;
    }
    return -1;
}

static void DrawToolbarButton(HDC hdc, RECT* rc, BOOL bHot, BOOL bPressed, BOOL bSelected) {
    COLORREF border = RGB(180, 180, 180);
    COLORREF fill = GetSysColor(COLOR_BTNFACE);

    if (bSelected) {
        fill = RGB(196, 224, 255);
        border = RGB(120, 170, 220);
    } else if (bPressed) {
        fill = RGB(204, 226, 247);
        border = RGB(100, 150, 200);
    } else if (bHot) {
        fill = RGB(232, 242, 252);
        border = RGB(170, 200, 230);
    }

    HBRUSH hFill = CreateSolidBrush(fill);
    FillRect(hdc, rc, hFill);
    DeleteBrush(hFill);

    HPEN hOldPen;
    HPEN hPen = CreatePenAndSelect(hdc, PS_SOLID, 1, border, &hOldPen);
    HBRUSH hOldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, rc->left, rc->top, rc->right, rc->bottom);
    SelectObject(hdc, hOldBrush);
    RestorePen(hdc, hOldPen);
    DeletePen(hPen);
}
LRESULT CALLBACK ToolbarWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        ToolbarOnPaint(hwnd);
        return 0;
    case WM_LBUTTONDOWN:
        ToolbarOnLButtonDown(hwnd, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_MOUSEMOVE:
        ToolbarOnMouseMove(hwnd, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_MOUSELEAVE:
        if (nHotIndex != -1) {
            nHotIndex = -1;
            InvalidateWindow(hwnd);
        }
        return 0;
    case WM_LBUTTONUP:
        ToolbarOnLButtonUp(hwnd, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DESTROY:
        if (hToolIcons) { DeleteObject(hToolIcons); hToolIcons = NULL; }
        if (hToolIconsExtra) { DeleteObject(hToolIconsExtra); hToolIconsExtra = NULL; }
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}
void CreateToolbar(HWND hParent) {
    WNDCLASS wndclass;
    static char szAppName[] = "PeztoldToolbar";
    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = ToolbarWndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInst;
    wndclass.hIcon = NULL;
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wndclass.lpszMenuName = NULL;
    wndclass.lpszClassName = szAppName;
    RegisterClass(&wndclass);
    hToolIcons = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP859));
    hToolIconsExtra = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP895));
    hToolbarWnd = CreateWindow(szAppName, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, TOOLBAR_WIDTH, 400,
        hParent, NULL, hInst, NULL);
}
HWND GetToolbarWindow() {
    return hToolbarWnd;
}
void ToolbarOnPaint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    ClearClientRect(hdc, hWnd, GetSysColorBrush(COLOR_BTNFACE));

    HDC hdcMem = CreateTempDC(hdc);
    if (!hdcMem) {
        EndPaint(hWnd, &ps);
        return;
    }
    HBITMAP hOldBmp = NULL;

    for (int i = 0; i < NUM_TOOLS; i++) {
        RECT rc;
        GetToolRect(i, &rc);

        BOOL bDrawPressed = FALSE;
        BOOL bHot = (i == nHotIndex);
        BOOL bSelected = (i == Tool_GetCurrent());

        if (i == nPressIndex) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);
            if (PtInRect(&rc, pt)) {
                bDrawPressed = TRUE;
            }
        }

        DrawToolbarButton(hdc, &rc, bHot, bDrawPressed, bSelected);

        if (hToolIcons || hToolIconsExtra) {
            HBITMAP hActiveBmp = NULL;
            int srcX = 0;
            if (i >= TOOL_PEN) {
                hActiveBmp = hToolIconsExtra;
                srcX = (i - TOOL_PEN) * ICON_WIDTH;
            } else {
                hActiveBmp = hToolIcons;
                srcX = i * ICON_WIDTH;
            }

            if (hActiveBmp) {
                int iconX = rc.left + (TOOLBTN_SIZE - ICON_WIDTH) / 2;
                int iconY = rc.top + (TOOLBTN_SIZE - ICON_HEIGHT) / 2;
                if (bDrawPressed) { iconX++; iconY++; }
                if (hOldBmp) SelectObject(hdcMem, hOldBmp);
                hOldBmp = SelectObject(hdcMem, hActiveBmp);
                TransparentBlt(hdc, iconX, iconY, ICON_WIDTH, ICON_HEIGHT,
                               hdcMem, srcX, 0, ICON_WIDTH, ICON_HEIGHT, ICON_COLORKEY);
            }
        }
    }

    if (hOldBmp) SelectObject(hdcMem, hOldBmp);
    DeleteTempDC(hdcMem);
    EndPaint(hWnd, &ps);
}
int GetToolFromPoint(int x, int y) {
    return HitTestTool(x, y);
}
void ToolbarOnLButtonDown(HWND hWnd, int x, int y) {
    int index = GetToolFromPoint(x, y);
    if (index != -1) {
        nPressIndex = index;
        SetCapture(hWnd);
        nHotIndex = index;
        InvalidateWindow(hWnd);
    }
}
void ToolbarOnMouseMove(HWND hWnd, int x, int y) {
    int hotIndex = GetToolFromPoint(x, y);
    if (hotIndex != nHotIndex) {
        nHotIndex = hotIndex;
        InvalidateWindow(hWnd);
    }

    if (nHotIndex != -1) {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        tme.dwHoverTime = 0;
        TrackMouseEvent(&tme);
    }
}
void ToolbarOnLButtonUp(HWND hWnd, int x, int y) {
    if (nPressIndex != -1) {
        int index = GetToolFromPoint(x, y);
        if (index == nPressIndex) {
            SetCurrentTool(index);
        }

        nPressIndex = -1;
        ReleaseCapture();
        InvalidateWindow(hWnd);
    }
}

int GetToolbarHeight(void) {
    int rows = (NUM_TOOLS + 1) / TOOLBAR_COLS;
    return TOOLBTN_PADDING + (rows * (TOOLBTN_SIZE + TOOLBTN_PADDING));
}
