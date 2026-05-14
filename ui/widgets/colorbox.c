#include "colorbox.h"
#include "../../peztold_core.h"

#include "../../draw.h"
#include "../../gdi_utils.h"
#include "../../helpers.h"
#include "../../palette.h"
#include "../../resource.h"
#include <commctrl.h>

static HWND hColorboxWnd = NULL;
static HFONT hColorboxFont = NULL;

static COLORREF paletteColors[28] = {
    RGB(0, 0, 0),       RGB(128, 128, 128), RGB(128, 0, 0),
    RGB(128, 128, 0),   RGB(0, 128, 0),     RGB(0, 128, 128),
    RGB(0, 0, 128),     RGB(128, 0, 128),   RGB(128, 128, 64),
    RGB(0, 64, 64),     RGB(0, 128, 255),   RGB(0, 64, 128),
    RGB(64, 0, 255),    RGB(128, 64, 0),    RGB(255, 255, 255),
    RGB(192, 192, 192), RGB(255, 0, 0),     RGB(255, 255, 0),
    RGB(0, 255, 0),     RGB(0, 255, 255),   RGB(0, 0, 255),
    RGB(255, 0, 255),   RGB(255, 255, 128), RGB(0, 255, 128),
    RGB(128, 255, 255), RGB(128, 128, 255), RGB(255, 0, 128),
    RGB(255, 128, 64)};

static const COLORREF paletteXP[28] = {
    RGB(0, 0, 0),       RGB(128, 128, 128), RGB(128, 0, 0),
    RGB(128, 128, 0),   RGB(0, 128, 0),     RGB(0, 128, 128),
    RGB(0, 0, 128),     RGB(128, 0, 128),   RGB(128, 128, 64),
    RGB(0, 64, 64),     RGB(0, 128, 255),   RGB(0, 64, 128),
    RGB(64, 0, 255),    RGB(128, 64, 0),    RGB(255, 255, 255),
    RGB(192, 192, 192), RGB(255, 0, 0),     RGB(255, 255, 0),
    RGB(0, 255, 0),     RGB(0, 255, 255),   RGB(0, 0, 255),
    RGB(255, 0, 255),   RGB(255, 255, 128), RGB(0, 255, 128),
    RGB(128, 255, 255), RGB(128, 128, 255), RGB(255, 0, 128),
    RGB(255, 128, 64)};

static const COLORREF paletteVista[28] = {
    RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),
    RGB(237, 28, 36),   RGB(255, 127, 39),  RGB(255, 242, 0),
    RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),
    RGB(163, 73, 164),  RGB(255, 255, 255), RGB(195, 195, 195),
    RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
    RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234),
    RGB(112, 146, 190), RGB(200, 191, 231), RGB(0, 0, 0),
    RGB(32, 32, 32),    RGB(64, 64, 64),    RGB(96, 96, 96),
    RGB(128, 128, 128), RGB(160, 160, 160), RGB(192, 192, 192),
    RGB(224, 224, 224)};

static const COLORREF paletteWin7[28] = {
    RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),
    RGB(237, 28, 36),   RGB(255, 127, 39),  RGB(255, 242, 0),
    RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),
    RGB(163, 73, 164),  RGB(255, 255, 255), RGB(195, 195, 195),
    RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
    RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234),
    RGB(112, 146, 190), RGB(200, 191, 231), RGB(255, 255, 255),
    RGB(195, 195, 195), RGB(185, 122, 87),  RGB(255, 174, 201),
    RGB(255, 201, 14),  RGB(239, 228, 176), RGB(181, 230, 29),
    RGB(153, 217, 234)};

#define COLORS_PER_ROW 14
#define CELL_SIZE 15
#define CELL_GAP 1
#define FGBG_BOX_SIZE 14
#define FGBG_OFFSET 4
#define PALETTE_START_X 36
#define PALETTE_START_Y 6

#define BTN_START_X (PALETTE_START_X + 14 * (CELL_SIZE + CELL_GAP) + 10)
#define BTN_WIDTH 44
#define BTN_HEIGHT 14
#define BTN_GAP 2

void ColorboxSyncCustomColors(void) { SetCustomColors(paletteColors); }

static int GetColorIndexAt(int x, int y) {
  int row, col, idx;
  int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;

  if (x < PALETTE_START_X)
    return -1;

  col = (x - PALETTE_START_X) / (CELL_SIZE + CELL_GAP);
  row = (y - nVertCenter) / (CELL_SIZE + CELL_GAP);

  if (row < 0 || row > 1 || col < 0 || col >= COLORS_PER_ROW)
    return -1;

  idx = row * COLORS_PER_ROW + col;
  if (idx >= 0 && idx < 28)
    return idx;
  return -1;
}

static BOOL IsFgBgBoxHit(int x, int y) {
  int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
  return (x >= FGBG_OFFSET && x <= FGBG_OFFSET + 24 && y >= nVertCenter &&
          y <= nVertCenter + 24);
}

void ColorboxOnPaint(HWND hWnd) {
  PAINTSTRUCT ps;
  HDC hdc;
  RECT rc;
  int i, row, col, x, y;
  int nVertCenter;

  hdc = BeginPaint(hWnd, &ps);

  // Fill background to prevent flicker (since we handle WM_ERASEBKGND)
  ClearClientRect(hdc, hWnd, GetSysColorBrush(COLOR_BTNFACE));

  nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;

  // Draw BG color box
  rc.left = FGBG_OFFSET + 7;
  rc.top = nVertCenter + 7;
  rc.right = rc.left + FGBG_BOX_SIZE;
  rc.bottom = rc.top + FGBG_BOX_SIZE;
  HBRUSH hBgBr = CreateSolidBrush(Palette_GetSecondaryColor());
  if (hBgBr) {
    FillRect(hdc, &rc, hBgBr);
    DeleteObject(hBgBr);
  }
  DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);

  // Draw FG color box
  rc.left = FGBG_OFFSET;
  rc.top = nVertCenter;
  rc.right = rc.left + FGBG_BOX_SIZE;
  rc.bottom = rc.top + FGBG_BOX_SIZE;
  HBRUSH hFgBr = CreateSolidBrush(Palette_GetPrimaryColor());
  if (hFgBr) {
    FillRect(hdc, &rc, hFgBr);
    DeleteObject(hFgBr);
  }
  DrawEdge(hdc, &rc, BDR_RAISEDINNER, BF_RECT);

  // Draw Palette
  for (i = 0; i < 28; i++) {
    row = i / COLORS_PER_ROW;
    col = i % COLORS_PER_ROW;
    x = PALETTE_START_X + col * (CELL_SIZE + CELL_GAP);
    y = nVertCenter + row * (CELL_SIZE + CELL_GAP);
    rc.left = x;
    rc.top = y;
    rc.right = x + CELL_SIZE;
    rc.bottom = y + CELL_SIZE;
    HBRUSH hPalBr = CreateSolidBrush(paletteColors[i]);
    if (hPalBr) {
      FillRect(hdc, &rc, hPalBr);
      DeleteObject(hPalBr);
    }
    DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);
  }

  // Draw Buttons
  if (!hColorboxFont) {
    hColorboxFont =
        CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                   OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                   DEFAULT_PITCH | FF_DONTCARE, "Arial");
  }
  HFONT hFont =
      hColorboxFont ? hColorboxFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  HFONT hOldFont = SelectObject(hdc, hFont);
  SetBkMode(hdc, TRANSPARENT);

  const char *labels[] = {"XP", "Vista", "Win7"};
  for (i = 0; i < 3; i++) {
    RECT rcBtn;
    rcBtn.left = BTN_START_X;
    rcBtn.top = 2 + i * (BTN_HEIGHT + BTN_GAP);
    rcBtn.right = rcBtn.left + BTN_WIDTH;
    rcBtn.bottom = rcBtn.top + BTN_HEIGHT;
    DrawThemedButton(hdc, &rcBtn, FALSE);
    DrawText(hdc, labels[i], -1, &rcBtn,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  }

  SelectObject(hdc, hOldFont);
  EndPaint(hWnd, &ps);
}

void ColorboxOnLButtonDown(HWND hWnd, int x, int y) {
  if (x >= BTN_START_X && x <= BTN_START_X + BTN_WIDTH) {
    int btnIdx = (y - 2) / (BTN_HEIGHT + BTN_GAP);
    if (btnIdx >= 0 && btnIdx < 3 && y >= 2 + btnIdx * (BTN_HEIGHT + BTN_GAP) &&
        y <= 2 + btnIdx * (BTN_HEIGHT + BTN_GAP) + BTN_HEIGHT) {
      if (btnIdx == 0)
        memcpy(paletteColors, paletteXP, sizeof(paletteColors));
      else if (btnIdx == 1)
        memcpy(paletteColors, paletteVista, sizeof(paletteColors));
      else
        memcpy(paletteColors, paletteWin7, sizeof(paletteColors));
      InvalidateWindow(hWnd);
      return;
    }
  }

  int idx = GetColorIndexAt(x, y);
  if (idx != -1) {
    Palette_SetPrimaryColor(paletteColors[idx]);
    InvalidateWindow(hWnd);
  }
}

void ColorboxOnRButtonDown(HWND hWnd, int x, int y) {
  int idx = GetColorIndexAt(x, y);
  if (idx != -1) {
    Palette_SetSecondaryColor(paletteColors[idx]);
    InvalidateWindow(hWnd);
  }
}

void ColorboxOnDoubleClick(HWND hWnd, int x, int y) {
  ColorboxSyncCustomColors();
  if (IsFgBgBoxHit(x, y)) {
    int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
    BOOL bHitFG = (x >= FGBG_OFFSET && x < FGBG_OFFSET + FGBG_BOX_SIZE &&
                   y >= nVertCenter && y < nVertCenter + FGBG_BOX_SIZE);
    COLORREF *pColor = bHitFG ? Palette_GetPrimaryColorPtr() : Palette_GetSecondaryColorPtr();
    COLORREF crNew = *pColor;
    if (ChooseColorDialog(hWnd, &crNew)) {
      *pColor = crNew;
      InvalidateWindow(hWnd);
    }
  } else {
    int idx = GetColorIndexAt(x, y);
    if (idx != -1) {
      COLORREF crNew = paletteColors[idx];
      if (ChooseColorDialog(hWnd, &crNew)) {
        paletteColors[idx] = crNew;
        Palette_SetPrimaryColor(crNew);
        InvalidateWindow(hWnd);
      }
    }
  }
}

LRESULT CALLBACK ColorboxWndProc(HWND hwnd, UINT message, WPARAM wParam,
                                 LPARAM lParam) {
  switch (message) {
  case WM_ERASEBKGND:
    return 1; // Prevent flicker - we paint the entire background in WM_PAINT
  case WM_PAINT:
    ColorboxOnPaint(hwnd);
    return 0;
  case WM_LBUTTONDOWN:
    ColorboxOnLButtonDown(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
    return 0;
  case WM_RBUTTONDOWN:
    ColorboxOnRButtonDown(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
    return 0;
  case WM_LBUTTONDBLCLK:
    ColorboxOnDoubleClick(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
    return 0;
  case WM_DESTROY:
    if (hColorboxFont) {
      DeleteObject(hColorboxFont);
      hColorboxFont = NULL;
    }
    return 0;
  }
  return DefWindowProc(hwnd, message, wParam, lParam);
}

void CreateColorbox(HWND hParent) {
  WNDCLASS wc;
  static char szClassName[] = "PeztoldColorbox";
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_DBLCLKS; // Removed CS_HREDRAW | CS_VREDRAW to reduce flicker
  wc.lpfnWndProc = ColorboxWndProc;
  wc.hInstance = hInst;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.lpszClassName = szClassName;
  RegisterClass(&wc);

  hColorboxWnd = CreateWindow(szClassName, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0,
                              COLORBOX_HEIGHT, hParent, NULL, hInst, NULL);
}

HWND GetColorboxWindow(void) { return hColorboxWnd; }
