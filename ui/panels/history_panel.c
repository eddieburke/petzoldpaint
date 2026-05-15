#include "history_panel.h"
#include "../../canvas.h"
#include "../../peztold_core.h"

#include "../../helpers.h"
#include "../../history.h"
#include "../../tools.h"
#include <commctrl.h>
#include <stdio.h>
#include <uxtheme.h>

#define HISTORY_PANEL_MIN_W 250
#define HISTORY_PANEL_MIN_H 300

// Control IDs
#define IDC_HISTORY_LIST 2001
#define IDC_GROUP_HISTORY 2010

static HWND hHistoryWnd = NULL;
static HWND hHistoryList = NULL;
static HWND hGroupHistory = NULL;
static HFONT hUiFont = NULL;

static void RefreshHistoryList(void) {
  if (!hHistoryList)
    return;

  int count = HistoryGetCount();
  int currentPos = HistoryGetPosition();
  int maxTextPx = 0;

  SendMessage(hHistoryList, WM_SETREDRAW, FALSE, 0);
  SendMessage(hHistoryList, LB_RESETCONTENT, 0, 0);

  HDC hdc = GetDC(hHistoryList);

  for (int i = 0; i < count; i++) {
    char desc[128];
    HistoryGetDescriptionAt(i, desc, sizeof(desc));
    if (desc[0] == '\0') {
      StringCchPrintf(desc, sizeof(desc), "Action %d", i + 1);
    }

    // Add indicator for current position
    char display[140];
    if (i == currentPos) {
      StringCchPrintf(display, sizeof(display), "> %s", desc);
    } else {
      StringCchCopy(display, sizeof(display), desc);
    }

    SendMessage(hHistoryList, LB_ADDSTRING, 0, (LPARAM)display);
    if (hdc) {
      SIZE sz;
      if (GetTextExtentPoint32(hdc, display, lstrlen(display), &sz) &&
          sz.cx > maxTextPx) {
        maxTextPx = sz.cx;
      }
    }
  }

  if (hdc)
    ReleaseDC(hHistoryList, hdc);

  SendMessage(hHistoryList, LB_SETHORIZONTALEXTENT, maxTextPx + 16, 0);
  SendMessage(hHistoryList, WM_SETREDRAW, TRUE, 0);
  InvalidateRect(hHistoryList, NULL, TRUE);

  // Select current position
  if (currentPos >= 0 && currentPos < count) {
    SendMessage(hHistoryList, LB_SETCURSEL, currentPos, 0);
  }
}

static void LayoutControls(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;
  if (width < HISTORY_PANEL_MIN_W)
    width = HISTORY_PANEL_MIN_W;
  if (height < HISTORY_PANEL_MIN_H)
    height = HISTORY_PANEL_MIN_H;

  int pad = 6;
  int groupPad = 8;
  int groupTitle = 16;
  int y = pad;

  int listGroupHeight = height - pad * 2;
  if (listGroupHeight < 100)
    listGroupHeight = 100;
  MoveWindow(hGroupHistory, pad, y, width - pad * 2, listGroupHeight, TRUE);
  int listHeight = listGroupHeight - groupTitle - groupPad;
  if (listHeight < 70)
    listHeight = 70;
  MoveWindow(hHistoryList, pad + groupPad / 2, y + groupTitle,
             width - pad * 2 - groupPad, listHeight, TRUE);
}

static void HandleHistorySelection(void) {
  int sel = (int)SendMessage(hHistoryList, LB_GETCURSEL, 0, 0);
  if (sel == LB_ERR)
    return;

  // Jump to selected history state
  HistoryJumpTo(sel);
  RefreshHistoryList();
}

static void HistoryPanel_OnCoreEvent(CoreEvent ev) {
  (void)ev;
  HistoryPanelSync();
}

static LRESULT CALLBACK HistoryPanelWndProc(HWND hwnd, UINT message,
                                            WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_CREATE: {
    struct {
      HWND *hwnd;
      DWORD exStyle;
      const char *cls;
      const char *text;
      DWORD style;
      int id;
    } controls[] = {{&hGroupHistory, 0, "BUTTON", "History (Last 100)",
                     WS_CHILD | WS_VISIBLE | BS_GROUPBOX, IDC_GROUP_HISTORY},
                    {&hHistoryList, WS_EX_CLIENTEDGE, "LISTBOX", "",
                     WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL |
                         WS_HSCROLL | LBS_NOINTEGRALHEIGHT,
                     IDC_HISTORY_LIST}};

    hUiFont = CreateSegoiUIFont(-11, FW_NORMAL);

    for (int i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
      *controls[i].hwnd =
          CreateWindowEx(controls[i].exStyle, controls[i].cls, controls[i].text,
                         controls[i].style, 0, 0, 10, 10, hwnd,
                         (HMENU)(INT_PTR)controls[i].id, hInst, NULL);
      if (hUiFont) {
        SendMessage(*controls[i].hwnd, WM_SETFONT, (WPARAM)hUiFont, TRUE);
      }
    }

    SetWindowTheme(hHistoryList, L"Explorer", NULL);

    RefreshHistoryList();
    LayoutControls(hwnd);
    Core_RegisterObserver(HistoryPanel_OnCoreEvent);
    return 0;
  }

  case WM_SIZE:
    LayoutControls(hwnd);
    return 0;

  case WM_GETMINMAXINFO: {
    MINMAXINFO *pInfo = (MINMAXINFO *)lParam;
    pInfo->ptMinTrackSize.x = HISTORY_PANEL_MIN_W;
    pInfo->ptMinTrackSize.y = HISTORY_PANEL_MIN_H;
  }
    return 0;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_HISTORY_LIST:
      if (HIWORD(wParam) == LBN_SELCHANGE) {
        HandleHistorySelection();
      }
      break;
    }
    return 0;

  case WM_DESTROY:
    if (hUiFont) {
      DeleteObject(hUiFont);
      hUiFont = NULL;
    }
    return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

void CreateHistoryPanel(HWND hParent) {
  WNDCLASS wc;
  static char szClassName[] = "PeztoldHistoryPanel";

  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = HistoryPanelWndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = szClassName;

  RegisterClass(&wc);

  hHistoryWnd =
      CreateWindowEx(WS_EX_TOOLWINDOW, szClassName, "History",
                     WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 400,
                     120, 280, 450, hParent, NULL, hInst, NULL);
  if (hHistoryWnd) {
    ShowWindow(hHistoryWnd, SW_SHOWNOACTIVATE);
  }
}

HWND GetHistoryPanelWindow(void) { return hHistoryWnd; }

void HistoryPanelSync(void) {
  if (!hHistoryWnd)
    return;
  RefreshHistoryList();
  InvalidateWindow(hHistoryWnd);
}
