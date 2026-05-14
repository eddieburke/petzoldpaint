#include "layers_panel.h"
#include "../../canvas.h"
#include "../../peztold_core.h"

#include "../../helpers.h"
#include "../../history.h"
#include "../../layers.h"
#include "../../resource.h"
#include "../../tools.h"
#include "history_panel.h"
#include <commctrl.h>
#include <stdio.h>
#include <uxtheme.h>

#define LAYERS_PANEL_MIN_W 200
#define LAYERS_PANEL_MIN_H 250

// Control IDs
#define IDC_LAYER_LIST 1001
#define IDC_BTN_ADD 1002
#define IDC_BTN_DELETE 1003
#define IDC_BTN_UP 1004
#define IDC_BTN_DOWN 1005
#define IDC_CHK_VISIBLE 1006
#define IDC_OPACITY_SLIDER 1008
#define IDC_BLEND_COMBO 1009
#define IDC_GROUP_LAYERS 1010
#define IDC_GROUP_PROPS 1011
#define IDC_BTN_MERGE 1012
#define IDC_OPACITY_LABEL 1013

static HWND hLayersWnd = NULL;
static HWND hLayerList = NULL;
static HWND hBtnAdd = NULL;
static HWND hBtnDelete = NULL;
static HWND hBtnUp = NULL;
static HWND hBtnDown = NULL;
static HWND hBtnMerge = NULL;
static HWND hChkVisible = NULL;
static HWND hOpacity = NULL;
static HWND hOpacityLabel = NULL;
static HWND hBlend = NULL;
static HWND hGroupLayers = NULL;
static HWND hGroupProps = NULL;
static HFONT hUiFont = NULL;
static int   s_startPercent = 100;
static BOOL  s_isDraggingOpacity = FALSE;

static int ListIndexToLayerIndex(int listIndex) {
  int count = LayersGetCount();
  return count - 1 - listIndex;
}

static int LayerIndexToListIndex(int layerIndex) {
  int count = LayersGetCount();
  return count - 1 - layerIndex;
}

static void UpdateOpacityLabel(int percent) {
  if (hOpacityLabel) {
    char buf[32];
    StringCchPrintf(buf, sizeof(buf), "Opacity: %d%%", percent);
    SetWindowText(hOpacityLabel, buf);
  }
}

static void UpdateControlsFromLayer(void) {
  int idx = LayersGetActiveIndex();
  if (idx < 0)
    return;

  SendMessage(hChkVisible, BM_SETCHECK,
              LayersGetVisible(idx) ? BST_CHECKED : BST_UNCHECKED, 0);

  int opacity = (int)LayersGetOpacity(idx);
  int percent = (opacity * 100) / 255;
  SendMessage(hOpacity, TBM_SETPOS, TRUE, percent);
  UpdateOpacityLabel(percent);

  int blendMode = LayersGetBlendMode(idx);
  SendMessage(hBlend, CB_SETCURSEL, blendMode, 0);
}

static void RefreshLayerList(void) {
  int count = LayersGetCount();
  SendMessage(hLayerList, LB_RESETCONTENT, 0, 0);
  for (int i = count - 1; i >= 0; i--) {
    char name[64];
    LayersGetName(i, name, sizeof(name));
    if (name[0] == '\0') {
      StringCchPrintf(name, sizeof(name), "Layer %d", i + 1);
    }
    SendMessage(hLayerList, LB_ADDSTRING, 0, (LPARAM)name);
  }
  int active = LayersGetActiveIndex();
  if (active >= 0 && active < count) {
    SendMessage(hLayerList, LB_SETCURSEL, LayerIndexToListIndex(active), 0);
  }
  UpdateControlsFromLayer();
}

static void LayoutControls(HWND hwnd) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  int width = rc.right - rc.left;
  int height = rc.bottom - rc.top;
  if (width < LAYERS_PANEL_MIN_W)
    width = LAYERS_PANEL_MIN_W;
  if (height < LAYERS_PANEL_MIN_H)
    height = LAYERS_PANEL_MIN_H;

  int pad = 6;
  int btnW = (width - pad * 5) / 4;
  int btnH = 22;
  int y = pad;
  int groupPad = 8;
  int groupTitle = 16;
  int propsMinHeight = 112;
  int totalBtnHeight = btnH * 2 + pad;

  int listGroupHeight = height - (pad * 3 + totalBtnHeight + propsMinHeight);
  if (listGroupHeight < 110)
    listGroupHeight = 110;
  MoveWindow(hGroupLayers, pad, y, width - pad * 2, listGroupHeight, TRUE);
  int listHeight = listGroupHeight - groupTitle - groupPad;
  if (listHeight < 70)
    listHeight = 70;
  MoveWindow(hLayerList, pad + groupPad / 2, y + groupTitle,
             width - pad * 2 - groupPad, listHeight, TRUE);
  y += listGroupHeight + pad;

  MoveWindow(hBtnAdd, pad, y, btnW, btnH, TRUE);
  MoveWindow(hBtnDelete, pad + btnW + pad, y, btnW, btnH, TRUE);
  MoveWindow(hBtnUp, pad + (btnW + pad) * 2, y, btnW, btnH, TRUE);
  MoveWindow(hBtnDown, pad + (btnW + pad) * 3, y, btnW, btnH, TRUE);
  y += btnH + pad;

  MoveWindow(hBtnMerge, pad, y, width - pad * 2, btnH, TRUE);
  y += btnH + pad;

  int propsHeight = height - y - pad;
  if (propsHeight < propsMinHeight)
    propsHeight = propsMinHeight;
  MoveWindow(hGroupProps, pad, y, width - pad * 2, propsHeight, TRUE);
  int innerY = y + groupTitle;
  int innerX = pad + groupPad / 2;
  int innerW = width - pad * 2 - groupPad;

  MoveWindow(hChkVisible, innerX, innerY, innerW, 18, TRUE);
  innerY += 18 + pad;

  MoveWindow(hOpacityLabel, innerX, innerY, innerW, 16, TRUE);
  innerY += 16;
  MoveWindow(hOpacity, innerX, innerY, innerW, 22, TRUE);
  innerY += 22 + pad;

  MoveWindow(hBlend, innerX, innerY, innerW, 200, TRUE);
}

static void HandleLayerSelection(void) {
  int listIdx = (int)SendMessage(hLayerList, LB_GETCURSEL, 0, 0);
  if (listIdx == LB_ERR)
    return;
  int idx = ListIndexToLayerIndex(listIdx);
  CommitSelection();
  if (LayersSetActiveIndex(idx)) {
    UpdateControlsFromLayer();
    InvalidateCanvas();
  }
}

static void HandleAddLayer(void) {
  Tool_FinalizeCurrentState();
  int count = LayersGetCount();
  char name[64];
  StringCchPrintf(name, sizeof(name), "Layer %d", count + 1);
  if (LayersAddLayer(name)) {
    HistoryPushFormatted("Add Layer: %s", name);
    RefreshLayerList();
    InvalidateCanvas();
    SetDocumentDirty();
  }
}

static void HandleDeleteLayer(void) {
  int listIdx = (int)SendMessage(hLayerList, LB_GETCURSEL, 0, 0);
  if (listIdx == LB_ERR)
    return;
  Tool_FinalizeCurrentState();
  int idx = ListIndexToLayerIndex(listIdx);
  if (LayersDeleteLayer(idx)) {
    HistoryPush("Delete Layer");
    RefreshLayerList();
    InvalidateCanvas();
    SetDocumentDirty();
  }
}

static void HandleMoveLayer(int delta) {
  int listIdx = (int)SendMessage(hLayerList, LB_GETCURSEL, 0, 0);
  if (listIdx == LB_ERR)
    return;
  int toList = listIdx + delta;
  if (toList < 0 || toList >= LayersGetCount())
    return;
  Tool_FinalizeCurrentState();
  int idx = ListIndexToLayerIndex(listIdx);
  int to = ListIndexToLayerIndex(toList);

  if (LayersMoveLayer(idx, to)) {
    HistoryPushLayerMove(idx, to);
    RefreshLayerList();
    InvalidateCanvas();
    SetDocumentDirty();
  }
}

static void HandleOpacityChange(BOOL bPushHistory) {
  int idx = LayersGetActiveIndex();
  if (idx < 0)
    return;
  int newPercent = (int)SendMessage(hOpacity, TBM_GETPOS, 0, 0);
  if (newPercent < 0)
    newPercent = 0;
  if (newPercent > 100)
    newPercent = 100;

  // Get old opacity before changing
  int oldPercent = ((int)LayersGetOpacity(idx) * 100) / 255;

  // Change the canvas real-time
  UpdateOpacityLabel(newPercent);
  BYTE opacity = (BYTE)((newPercent * 255) / 100);
  LayersSetOpacity(idx, opacity);
  InvalidateCanvas();
  SetDocumentDirty();
 
  // Only push history when drag ends and value changed
  if (bPushHistory && s_startPercent != newPercent) {
    HistoryPushLayerOpacity(idx, s_startPercent, newPercent);
    s_startPercent = newPercent;
  }
}

static void HandleBlendChange(void) {
  int idx = LayersGetActiveIndex();
  if (idx < 0)
    return;
  int newMode = (int)SendMessage(hBlend, CB_GETCURSEL, 0, 0);
  if (newMode == CB_ERR)
    return;

  // Get old blend mode before changing
  int oldMode = LayersGetBlendMode(idx);

  // Only push history if actually changed
  if (oldMode != newMode) {
    LayersSetBlendMode(idx, newMode);
    InvalidateCanvas();
    SetDocumentDirty();
  }
}

static void HandleVisibleToggle(void) {
  int idx = LayersGetActiveIndex();
  if (idx < 0)
    return;
  BOOL newVisible =
      (SendMessage(hChkVisible, BM_GETCHECK, 0, 0) == BST_CHECKED);

  // Get old visibility before changing
  BOOL oldVisible = LayersGetVisible(idx);

  // Only push history if actually changed
  if (oldVisible != newVisible) {
    LayersSetVisible(idx, newVisible);
    HistoryPushLayerVisibility(idx, oldVisible, newVisible);
    InvalidateCanvas();
    SetDocumentDirty();
  }
}

static LRESULT CALLBACK LayersPanelWndProc(HWND hwnd, UINT message,
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
    } controls[] = {
        {&hGroupLayers, 0, "BUTTON", "Layers",
         WS_CHILD | WS_VISIBLE | BS_GROUPBOX, IDC_GROUP_LAYERS},
        {&hGroupProps, 0, "BUTTON", "Layer Options",
         WS_CHILD | WS_VISIBLE | BS_GROUPBOX, IDC_GROUP_PROPS},
        {&hLayerList, WS_EX_CLIENTEDGE, "LISTBOX", "",
         WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, IDC_LAYER_LIST},
        {&hBtnAdd, 0, "BUTTON", "Add", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
         IDC_BTN_ADD},
        {&hBtnDelete, 0, "BUTTON", "Del", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
         IDC_BTN_DELETE},
        {&hBtnUp, 0, "BUTTON", "Up", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
         IDC_BTN_UP},
        {&hBtnDown, 0, "BUTTON", "Down", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
         IDC_BTN_DOWN},
        {&hBtnMerge, 0, "BUTTON", "Merge Down",
         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, IDC_BTN_MERGE},
        {&hChkVisible, 0, "BUTTON", "Visible",
         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, IDC_CHK_VISIBLE},
        {&hOpacityLabel, 0, "STATIC", "Opacity: 100%", WS_CHILD | WS_VISIBLE,
         IDC_OPACITY_LABEL},
        {&hOpacity, 0, TRACKBAR_CLASS, "",
         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS, IDC_OPACITY_SLIDER},
        {&hBlend, 0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
         IDC_BLEND_COMBO}};

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

    SendMessage(hOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    SendMessage(hOpacity, TBM_SETTICFREQ, 10, 0);

    const char *blendModes[] = {"Normal", "Multiply", "Screen", "Overlay"};
    for (int i = 0; i < (int)(sizeof(blendModes) / sizeof(blendModes[0]));
         i++) {
      SendMessage(hBlend, CB_ADDSTRING, 0, (LPARAM)blendModes[i]);
    }
    SendMessage(hBlend, CB_SETCURSEL, 0, 0);

    SetWindowTheme(hLayerList, L"Explorer", NULL);
    SetWindowTheme(hBlend, L"Explorer", NULL);

    RefreshLayerList();
    LayoutControls(hwnd);
    return 0;
  }

  case WM_SIZE:
    LayoutControls(hwnd);
    return 0;

  case WM_GETMINMAXINFO: {
    MINMAXINFO *pInfo = (MINMAXINFO *)lParam;
    pInfo->ptMinTrackSize.x = LAYERS_PANEL_MIN_W;
    pInfo->ptMinTrackSize.y = LAYERS_PANEL_MIN_H;
  }
    return 0;

  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case IDC_LAYER_LIST:
      if (HIWORD(wParam) == LBN_SELCHANGE) {
        HandleLayerSelection();
      }
      break;
    case IDC_BTN_ADD:
      HandleAddLayer();
      break;
    case IDC_BTN_DELETE:
      HandleDeleteLayer();
      break;
    case IDC_BTN_UP:
      // Move layer up in z-order: moving UP in list = higher layer index
      HandleMoveLayer(-1);
      break;
    case IDC_BTN_DOWN:
      // Move layer down in z-order: moving DOWN in list = lower layer index
      HandleMoveLayer(1);
      break;
    case IDC_BTN_MERGE:
      Tool_FinalizeCurrentState();
      if (LayersMergeDown(LayersGetActiveIndex())) {
        HistoryPush("Merge Layer");
        RefreshLayerList();
        InvalidateCanvas();
        SetDocumentDirty();
      }
      break;
    case IDC_CHK_VISIBLE:
      HandleVisibleToggle();
      break;
    case IDC_BLEND_COMBO:
      if (HIWORD(wParam) == CBN_SELCHANGE) {
        HandleBlendChange();
      }
      break;
    }
    return 0;

  case WM_HSCROLL:
    if ((HWND)lParam == hOpacity) {
      if (!s_isDraggingOpacity) {
        int idx = LayersGetActiveIndex();
        if (idx >= 0) {
          s_startPercent = (int)SendMessage(hOpacity, TBM_GETPOS, 0, 0);
          s_isDraggingOpacity = TRUE;
        }
      }

      BOOL bEndTrack = (LOWORD(wParam) == TB_ENDTRACK) ||
                       (LOWORD(wParam) == TB_THUMBPOSITION);
      
      if (bEndTrack) {
        s_isDraggingOpacity = FALSE;
      }

      HandleOpacityChange(bEndTrack);
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

void CreateLayersPanel(HWND hParent) {
  WNDCLASS wc;
  static char szClassName[] = "PeztoldLayersPanel";

  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = LayersPanelWndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = szClassName;

  RegisterClass(&wc);

  hLayersWnd =
      CreateWindowEx(WS_EX_TOOLWINDOW, szClassName, NULL,
                     WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU, 120,
                     120, 250, 400, hParent, NULL, hInst, NULL);
  if (hLayersWnd) {
    ShowWindow(hLayersWnd, SW_SHOWNOACTIVATE);
  }
}

HWND GetLayersPanelWindow(void) { return hLayersWnd; }

void LayersPanelSync(void) {
  if (!hLayersWnd)
    return;
  RefreshLayerList();
  InvalidateWindow(hLayersWnd);
}
