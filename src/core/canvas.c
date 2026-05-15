/*------------------------------------------------------------
   CANVAS.C -- Canvas Window and Rendering Implementation

   This module implements the main canvas window, which displays
   the composite of all layers and handles user interaction.
  ------------------------------------------------------------*/

#include "peztold_core.h"
#include "controller.h"
#include "cursors.h"
#include "gdi_utils.h"
#include "geom.h"
#include "history.h"
#include "layers.h"
#include "pixel_ops.h"
#include "file_io.h"
#include "tools.h"
#include "canvas.h"
#include "tools/selection_tool.h"
#include "ui/widgets/statusbar.h"
#include "ui/widgets/toolbar.h"

#include <math.h>
#include <stdio.h>

/*------------------------------------------------------------
   Global Variables
  ------------------------------------------------------------*/

static HWND hCanvasWnd = NULL;

static HDC s_hViewDC = NULL;
static HBITMAP s_hViewBmp = NULL;
static HBITMAP s_hOldViewBmp = NULL;
static int s_viewW = 0;
static int s_viewH = 0;

static void CleanupViewBuffer(void) {
  if (s_hViewDC) {
    if (s_hOldViewBmp) {
      SelectObject(s_hViewDC, s_hOldViewBmp);
      s_hOldViewBmp = NULL;
    }
    DeleteDC(s_hViewDC);
    s_hViewDC = NULL;
  }
  if (s_hViewBmp) {
    DeleteObject(s_hViewBmp);
    s_hViewBmp = NULL;
  }
  s_viewW = s_viewH = 0;
}

static BOOL EnsureViewBuffer(HDC hdc, int w, int h) {
  if (s_hViewDC && s_viewW == w && s_viewH == h)
    return TRUE;

  if (w <= 0 || h <= 0)
    return FALSE;

  if (!s_hViewDC) {
    s_hViewDC = CreateCompatibleDC(hdc);
  }

  if (s_viewW != w || s_viewH != h) {
    if (s_hOldViewBmp) {
      SelectObject(s_hViewDC, s_hOldViewBmp);
      s_hOldViewBmp = NULL;
    }
    if (s_hViewBmp) {
      DeleteObject(s_hViewBmp);
    }
    s_hViewBmp = CreateCompatibleBitmap(hdc, w, h);
    if (!s_hViewBmp)
      return FALSE;
    s_hOldViewBmp = (HBITMAP)SelectObject(s_hViewDC, s_hViewBmp);
    s_viewW = w;
    s_viewH = h;
  }
  return TRUE;
}

int Canvas_GetWidth(void) { return Doc_GetWidth(); }
void Canvas_SetWidth(int w) { Doc_SetSize(w, Doc_GetHeight()); }
int Canvas_GetHeight(void) { return Doc_GetHeight(); }
void Canvas_SetHeight(int h) { Doc_SetSize(Doc_GetWidth(), h); }
int Canvas_GetScrollX(void) { return Doc_GetScrollX(); }
void Canvas_SetScrollX(int x) { Document *d = GetDocument(); d->scrollX = x; }
int* Canvas_GetScrollXPtr(void) { return &GetDocument()->scrollX; }
int Canvas_GetScrollY(void) { return Doc_GetScrollY(); }
void Canvas_SetScrollY(int y) { Document *d = GetDocument(); d->scrollY = y; }
int* Canvas_GetScrollYPtr(void) { return &GetDocument()->scrollY; }
double Canvas_GetZoom(void) { return Doc_GetZoom(); }
void Canvas_SetZoom(double z) { 
  Doc_SetZoom(z); 
  StatusBarUpdateZoom(z);
}



/*------------------------------------------------------------
   Forward Declarations
  ------------------------------------------------------------*/

static void DrawResizeHandles(HDC hdc, int nDestX, int nDestY, int nScaledW,
                              int nScaledH);
LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT message, WPARAM wParam,
                               LPARAM lParam);

/*------------------------------------------------------------
   Canvas Management Functions
  ------------------------------------------------------------*/

BOOL CreateCanvas(int width, int height) {
  if (!LayersInit(width, height))
    return FALSE;

  Canvas_SetWidth(width);
  Canvas_SetHeight(height);

  return TRUE;
}

BOOL ResizeCanvas(int nNewWidth, int nNewHeight) {
  if (nNewWidth <= 0 || nNewHeight <= 0)
    return FALSE;

  if (!LayersResize(nNewWidth, nNewHeight))
    return FALSE;
  Canvas_SetWidth(nNewWidth);
  Canvas_SetHeight(nNewHeight);
  return TRUE;
}

void DestroyCanvas(void) {
  /* View buffer is released in the canvas window WM_DESTROY (CleanupViewBuffer). */
  LayersDestroy();
  HistoryDestroy();
  FileIO_ShutdownCom();
}



void ClearCanvas(COLORREF color) {
  BYTE *bits = Layers_BeginWrite();
  if (!bits)
    return;

  PixelOps_Fill(bits, Canvas_GetWidth(), Canvas_GetHeight(), color, 255);

  LayersMarkDirty();
  SetDocumentDirty();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

/*------------------------------------------------------------
   Undo/Redo Functions
  ------------------------------------------------------------*/

BOOL Undo(void) {
  return HistoryUndo();
}

BOOL Redo(void) {
  return HistoryRedo();
}

void GetCanvasViewportOrigin(int *pX, int *pY) {
  if (pX)
    *pX = 2 - Canvas_GetScrollX();
  if (pY)
    *pY = 2 - Canvas_GetScrollY();
}

static void DrawResizeHandles(HDC hdc, int nDestX, int nDestY, int nScaledW,
                               int nScaledH) {
  if (IsSelectionActive())
    return;
  HBRUSH hOldBrush;
  HBRUSH hBrush = CreateBrushAndSelect(hdc, RGB(0, 0, 0), &hOldBrush);

  Rectangle(hdc, nDestX + nScaledW + 2, nDestY + nScaledH + 2,
            nDestX + nScaledW + 2 + HANDLE_SIZE,
            nDestY + nScaledH + 2 + HANDLE_SIZE);

  Rectangle(hdc, nDestX + nScaledW + 2, nDestY + nScaledH / 2 - HANDLE_SIZE / 2,
            nDestX + nScaledW + 2 + HANDLE_SIZE,
            nDestY + nScaledH / 2 + HANDLE_SIZE / 2);

  Rectangle(hdc, nDestX + nScaledW / 2 - HANDLE_SIZE / 2, nDestY + nScaledH + 2,
            nDestX + nScaledW / 2 + HANDLE_SIZE / 2,
            nDestY + nScaledH + 2 + HANDLE_SIZE);

  SelectObject(hdc, hOldBrush);
  Gdi_DeleteBrush(hBrush);
}

/*------------------------------------------------------------
   Canvas Window Procedure
  ------------------------------------------------------------*/

LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  switch (message) {
  case WM_SIZE:
    Controller_HandleSize(hwnd);
    return 0;

  case WM_DESTROY:
    CleanupViewBuffer();
    return 0;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc;
    HDC hBmpDC = NULL;
    HBITMAP hOldBmp = NULL;
    RECT rcClient;
    int nScaledW, nScaledH;

    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rcClient);

    if (!EnsureViewBuffer(hdc, rcClient.right, rcClient.bottom)) {
      EndPaint(hwnd, &ps);
      return 0;
    }

    FillRect(s_hViewDC, &rcClient, GetSysColorBrush(COLOR_APPWORKSPACE));

    GetScaledDimensions(Canvas_GetWidth(), Canvas_GetHeight(), &nScaledW, &nScaledH);

    int nDestX, nDestY;
    GetCanvasViewportOrigin(&nDestX, &nDestY);
    double dScale = GetZoomScale();

    RECT rcBorder = {nDestX - 1, nDestY - 1, nDestX + nScaledW + 1,
                     nDestY + nScaledH + 1};
    FrameRect(s_hViewDC, &rcBorder, (HBRUSH)GetStockObject(BLACK_BRUSH));

    SetStretchBltMode(s_hViewDC, COLORONCOLOR);

    HBITMAP hComposite = LayersGetCompositeBitmap(TRUE);
    if (hComposite) {
      hBmpDC = CreateCompatibleDC(hdc);
      if (hBmpDC) {
        hOldBmp = (HBITMAP)SelectObject(hBmpDC, hComposite);
        StretchBlt(s_hViewDC, nDestX, nDestY, nScaledW, nScaledH, hBmpDC, 0, 0,
                   Canvas_GetWidth(), Canvas_GetHeight(), SRCCOPY);
        SelectObject(hBmpDC, hOldBmp);
        DeleteDC(hBmpDC);
        hBmpDC = NULL;
        hOldBmp = NULL;
      }
    }

    ToolDrawOverlay(s_hViewDC, dScale, nDestX, nDestY);
    DrawResizeHandles(s_hViewDC, nDestX, nDestY, nScaledW, nScaledH);

    /* Resize preview from controller */
    if (Controller_IsResizing()) {
      int prevW, prevH;
      Controller_GetResizePreview(&prevW, &prevH);
      if (prevW != Canvas_GetWidth() || prevH != Canvas_GetHeight()) {
        int nPreviewW, nPreviewH;
        GetScaledDimensions(prevW, prevH, &nPreviewW, &nPreviewH);

        HPEN hOldPen;
        HPEN hPenDotted =
            CreatePenAndSelect(s_hViewDC, PS_DOT, 1, RGB(0, 0, 0), &hOldPen);
        HBRUSH hOldBrush2 =
            (HBRUSH)SelectObject(s_hViewDC, GetStockObject(NULL_BRUSH));

        Rectangle(s_hViewDC, nDestX, nDestY, nDestX + nPreviewW,
                  nDestY + nPreviewH);

        RestorePen(s_hViewDC, hOldPen);
        SelectObject(s_hViewDC, hOldBrush2);
        Gdi_DeletePen(hPenDotted);
      }
    }

    BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
           ps.rcPaint.right - ps.rcPaint.left,
           ps.rcPaint.bottom - ps.rcPaint.top, s_hViewDC, ps.rcPaint.left,
           ps.rcPaint.top, SRCCOPY);

    EndPaint(hwnd, &ps);
  }
    return 0;

  case WM_ERASEBKGND:
    return 1;

  case WM_SETCURSOR:
    if (LOWORD(lParam) == HTCLIENT) {
      POINT pt;
      GetCursorPos(&pt);
      ScreenToClient(hwnd, &pt);
      Controller_HandleSetCursor(hwnd, pt.x, pt.y);
      return TRUE;
    }
    break;

  case WM_LBUTTONDOWN:
    Controller_HandleMouseDown(hwnd, GET_X_LPARAM(lParam),
                               GET_Y_LPARAM(lParam), MK_LBUTTON);
    return 0;

  case WM_LBUTTONDBLCLK:
    Controller_HandleDoubleClick(hwnd, GET_X_LPARAM(lParam),
                                 GET_Y_LPARAM(lParam), MK_LBUTTON);
    return 0;

  case WM_RBUTTONDOWN:
    Controller_HandleMouseDown(hwnd, GET_X_LPARAM(lParam),
                               GET_Y_LPARAM(lParam), MK_RBUTTON);
    return 0;

  case WM_MOUSEMOVE:
    Controller_HandleMouseMove(hwnd, GET_X_LPARAM(lParam),
                               GET_Y_LPARAM(lParam), (int)wParam);
    return 0;

  case WM_LBUTTONUP:
    Controller_HandleMouseUp(hwnd, GET_X_LPARAM(lParam),
                             GET_Y_LPARAM(lParam), MK_LBUTTON);
    return 0;

  case WM_RBUTTONUP:
    Controller_HandleMouseUp(hwnd, GET_X_LPARAM(lParam),
                             GET_Y_LPARAM(lParam), MK_RBUTTON);
    return 0;

  case WM_CAPTURECHANGED:
    Controller_HandleCaptureLost(hwnd);
    return 0;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYDOWN:
  case WM_KEYUP:
    Controller_HandleKey(hwnd, wParam, message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
    return 0;

  case WM_TIMER:
    Controller_HandleTimer(hwnd, wParam);
    return 0;

  case WM_MOUSEWHEEL:
    Controller_HandleMouseWheel(hwnd, wParam, lParam);
    return 0;

  case WM_HSCROLL:
    Controller_HandleScroll(hwnd, SB_HORZ, LOWORD(wParam));
    return 0;

  case WM_VSCROLL:
    Controller_HandleScroll(hwnd, SB_VERT, LOWORD(wParam));
    return 0;
  }

  return DefWindowProc(hwnd, message, wParam, lParam);
}

/*------------------------------------------------------------
   Canvas Window Management Functions
  ------------------------------------------------------------*/

void CreateCanvasWindow(HWND hParent) {
  hCanvasWnd = NULL; // Initialize window handle
  WNDCLASS wc;
  static char szClassName[] = "PeztoldCanvas";

  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
  wc.lpfnWndProc = CanvasWndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = szClassName;

  RegisterClass(&wc);

  hCanvasWnd = CreateWindow(szClassName, NULL,
                            WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL, 0,
                            0, 100, 100, hParent, NULL, hInst, NULL);
}

HWND GetCanvasWindow(void) { return hCanvasWnd; }



void ResetCanvasScroll(void) {
  Canvas_SetScrollX(0);
  Canvas_SetScrollY(0);
  if (hCanvasWnd) {
    Controller_UpdateScrollbars(hCanvasWnd);
    InvalidateRect(hCanvasWnd, NULL, FALSE);
  }
}

void Canvas_ApplyZoomCentered(double newZoom) {
  if (hCanvasWnd) {
    RECT rcCanvas;
    GetClientRect(hCanvasWnd, &rcCanvas);
    int ptMouseX = (rcCanvas.right - rcCanvas.left) / 2;
    int ptMouseY = (rcCanvas.bottom - rcCanvas.top) / 2;
    Canvas_ZoomAroundPoint(newZoom, ptMouseX, ptMouseY);
  }
}

void Canvas_ZoomAroundPoint(double newZoom, int ptMouseX, int ptMouseY) {
  if (newZoom < MIN_ZOOM_PERCENT) newZoom = MIN_ZOOM_PERCENT;
  if (newZoom > MAX_ZOOM_PERCENT) newZoom = MAX_ZOOM_PERCENT;
  
  if (newZoom != Canvas_GetZoom() && hCanvasWnd) {
    double oldScale = Canvas_GetZoom() / 100.0;
    double newScale = newZoom / 100.0;

    int nDestX, nDestY;
    GetCanvasViewportOrigin(&nDestX, &nDestY);

    double xBmp = (double)(ptMouseX + 0.5 - nDestX) / oldScale;
    double yBmp = (double)(ptMouseY + 0.5 - nDestY) / oldScale;

    Canvas_SetZoom(newZoom);
    Controller_UpdateScrollbars(hCanvasWnd);
    GetCanvasViewportOrigin(&nDestX, &nDestY);

    int newXScreen = (int)floor(xBmp * newScale - 0.5) + nDestX;
    int newYScreen = (int)floor(yBmp * newScale - 0.5) + nDestY;

    Canvas_SetScrollX(Canvas_GetScrollX() + (newXScreen - ptMouseX));
    Canvas_SetScrollY(Canvas_GetScrollY() + (newYScreen - ptMouseY));

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE;
    
    GetScrollInfo(hCanvasWnd, SB_HORZ, &si);
    Canvas_SetScrollX(max(0, min(Canvas_GetScrollX(), (int)(si.nMax - (int)si.nPage + 1))));

    GetScrollInfo(hCanvasWnd, SB_VERT, &si);
    Canvas_SetScrollY(max(0, min(Canvas_GetScrollY(), (int)(si.nMax - (int)si.nPage + 1))));

    si.fMask = SIF_POS;
    si.nPos = Canvas_GetScrollX();
    SetScrollInfo(hCanvasWnd, SB_HORZ, &si, TRUE);
    si.nPos = Canvas_GetScrollY();
    SetScrollInfo(hCanvasWnd, SB_VERT, &si, TRUE);

    SendMessage(GetParent(hCanvasWnd), WM_SIZE, 0, 0);
    ToolHandleLifecycleEvent(TOOL_LIFECYCLE_VIEWPORT_CHANGED, hCanvasWnd);
    InvalidateRect(hCanvasWnd, NULL, FALSE);
  }
}
