#include "colorbox.h"
#include "peztold_core.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "palette.h"
#include "resource.h"
#include <commctrl.h>
static HWND hColorboxWnd = NULL;
static HFONT hColorboxFont = NULL;
static BOOL s_opacityDrag = FALSE;
typedef enum { COLOR_TARGET_PRIMARY = 0, COLOR_TARGET_SECONDARY = 1 } ColorTarget;
static ColorTarget s_activeTarget = COLOR_TARGET_PRIMARY;
static COLORREF paletteColors[28] = {RGB(0, 0, 0), RGB(128, 128, 128), RGB(128, 0, 0), RGB(128, 128, 0), RGB(0, 128, 0), RGB(0, 128, 128), RGB(0, 0, 128), RGB(128, 0, 128), RGB(128, 128, 64), RGB(0, 64, 64), RGB(0, 128, 255), RGB(0, 64, 128), RGB(64, 0, 255), RGB(128, 64, 0), RGB(255, 255, 255), RGB(192, 192, 192), RGB(255, 0, 0), RGB(255, 255, 0), RGB(0, 255, 0), RGB(0, 255, 255), RGB(0, 0, 255), RGB(255, 0, 255), RGB(255, 255, 128), RGB(0, 255, 128), RGB(128, 255, 255), RGB(128, 128, 255), RGB(255, 0, 128), RGB(255, 128, 64)};
static const COLORREF paletteXP[28] = {RGB(0, 0, 0), RGB(128, 128, 128), RGB(128, 0, 0), RGB(128, 128, 0), RGB(0, 128, 0), RGB(0, 128, 128), RGB(0, 0, 128), RGB(128, 0, 128), RGB(128, 128, 64), RGB(0, 64, 64), RGB(0, 128, 255), RGB(0, 64, 128), RGB(64, 0, 255), RGB(128, 64, 0), RGB(255, 255, 255), RGB(192, 192, 192), RGB(255, 0, 0), RGB(255, 255, 0), RGB(0, 255, 0), RGB(0, 255, 255), RGB(0, 0, 255), RGB(255, 0, 255), RGB(255, 255, 128), RGB(0, 255, 128), RGB(128, 255, 255), RGB(128, 128, 255), RGB(255, 0, 128), RGB(255, 128, 64)};
static const COLORREF paletteVista[28] = {RGB(0, 0, 0), RGB(127, 127, 127), RGB(136, 0, 21), RGB(237, 28, 36), RGB(255, 127, 39), RGB(255, 242, 0), RGB(34, 177, 76), RGB(0, 162, 232), RGB(63, 72, 204), RGB(163, 73, 164), RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87), RGB(255, 174, 201), RGB(255, 201, 14), RGB(239, 228, 176), RGB(181, 230, 29), RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231), RGB(0, 0, 0), RGB(32, 32, 32), RGB(64, 64, 64), RGB(96, 96, 96), RGB(128, 128, 128), RGB(160, 160, 160), RGB(192, 192, 192), RGB(224, 224, 224)};
static const COLORREF paletteWin7[28] = {RGB(0, 0, 0), RGB(127, 127, 127), RGB(136, 0, 21), RGB(237, 28, 36), RGB(255, 127, 39), RGB(255, 242, 0), RGB(34, 177, 76), RGB(0, 162, 232), RGB(63, 72, 204), RGB(163, 73, 164), RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87), RGB(255, 174, 201), RGB(255, 201, 14), RGB(239, 228, 176), RGB(181, 230, 29), RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231), RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87), RGB(255, 174, 201), RGB(255, 201, 14), RGB(239, 228, 176), RGB(181, 230, 29), RGB(153, 217, 234)};
#define COLORS_PER_ROW            14
#define CELL_SIZE                 15
#define CELL_GAP                  1
#define FGBG_BOX_SIZE             14
#define FGBG_OFFSET               4
#define PALETTE_START_X           36
#define PALETTE_START_Y           6
#define BTN_START_X               (PALETTE_START_X + 14 * (CELL_SIZE + CELL_GAP) + 10)
#define BTN_WIDTH                 44
#define BTN_HEIGHT                14
#define BTN_GAP                   2
#define OPACITY_BAR_HEIGHT        8
#define OPACITY_BAR_BOTTOM_MARGIN 5
void ColorboxSyncCustomColors(void) {
	SetCustomColors(paletteColors);
}
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
	return (x >= FGBG_OFFSET && x <= FGBG_OFFSET + 24 && y >= nVertCenter && y <= nVertCenter + 24);
}
static BOOL IsPrimaryBoxHit(int x, int y) {
	int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
	return (x >= FGBG_OFFSET && x < FGBG_OFFSET + FGBG_BOX_SIZE && y >= nVertCenter && y < nVertCenter + FGBG_BOX_SIZE);
}
static BOOL IsSecondaryBoxHit(int x, int y) {
	int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
	return (x >= FGBG_OFFSET + 7 && x < FGBG_OFFSET + 7 + FGBG_BOX_SIZE && y >= nVertCenter + 7 && y < nVertCenter + 7 + FGBG_BOX_SIZE);
}
static RECT GetOpacityBarRect(HWND hWnd) {
	RECT rcClient;
	GetClientRect(hWnd, &rcClient);
	RECT rc;
	rc.left = PALETTE_START_X;
	rc.right = rcClient.right - 6;
	if (rc.right < rc.left + 32)
		rc.right = rc.left + 32;
	rc.bottom = COLORBOX_HEIGHT - OPACITY_BAR_BOTTOM_MARGIN;
	rc.top = rc.bottom - OPACITY_BAR_HEIGHT;
	return rc;
}
static BYTE GetTargetOpacity(ColorTarget target) {
	return (target == COLOR_TARGET_SECONDARY) ? Palette_GetSecondaryOpacity() : Palette_GetPrimaryOpacity();
}
static void SetTargetOpacity(ColorTarget target, BYTE opacity) {
	if (target == COLOR_TARGET_SECONDARY)
		Palette_SetSecondaryOpacity(opacity);
	else
		Palette_SetPrimaryOpacity(opacity);
}
static COLORREF GetTargetColor(ColorTarget target) {
	return (target == COLOR_TARGET_SECONDARY) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
}
static BYTE BlendChan(BYTE src, BYTE bg, BYTE alpha) {
	return (BYTE)(((int)src * (int)alpha + (int)bg * (255 - (int)alpha) + 127) / 255);
}
static COLORREF BlendColorOnBg(COLORREF color, BYTE alpha, COLORREF bg) {
	return RGB(BlendChan(GetRValue(color), GetRValue(bg), alpha), BlendChan(GetGValue(color), GetGValue(bg), alpha), BlendChan(GetBValue(color), GetBValue(bg), alpha));
}
static void DrawChecker(HDC hdc, const RECT *rc, int cellSize) {
	for (int y = rc->top; y < rc->bottom; y += cellSize) {
		for (int x = rc->left; x < rc->right; x += cellSize) {
			RECT tile = {x, y, min(x + cellSize, rc->right), min(y + cellSize, rc->bottom)};
			BOOL dark = (((x - rc->left) / cellSize) + ((y - rc->top) / cellSize)) & 1;
			HBRUSH h = CreateSolidBrush(dark ? RGB(180, 180, 180) : RGB(230, 230, 230));
			if (h) {
				FillRect(hdc, &tile, h);
				DeleteObject(h);
			}
		}
	}
}
static void DrawAlphaSwatch(HDC hdc, const RECT *rc, COLORREF color, BYTE alpha) {
	DrawChecker(hdc, rc, 4);
	for (int y = rc->top; y < rc->bottom; y += 4) {
		for (int x = rc->left; x < rc->right; x += 4) {
			RECT tile = {x, y, min(x + 4, rc->right), min(y + 4, rc->bottom)};
			BOOL dark = (((x - rc->left) / 4) + ((y - rc->top) / 4)) & 1;
			COLORREF bg = dark ? RGB(180, 180, 180) : RGB(230, 230, 230);
			HBRUSH h = CreateSolidBrush(BlendColorOnBg(color, alpha, bg));
			if (h) {
				FillRect(hdc, &tile, h);
				DeleteObject(h);
			}
		}
	}
}
static void DrawOpacityBar(HDC hdc, HWND hWnd) {
	RECT rc = GetOpacityBarRect(hWnd);
	if (rc.right <= rc.left || rc.bottom <= rc.top)
		return;
	int width = rc.right - rc.left;
	DrawChecker(hdc, &rc, 4);
	COLORREF color = GetTargetColor(s_activeTarget);
	for (int i = 0; i < width; i++) {
		BYTE alpha = (width <= 1) ? 255 : (BYTE)((i * 255) / (width - 1));
		int x = rc.left + i;
		for (int y = rc.top; y < rc.bottom; y += 4) {
			RECT px = {x, y, x + 1, min(y + 4, rc.bottom)};
			BOOL dark = (((x - rc.left) / 4) + ((y - rc.top) / 4)) & 1;
			COLORREF bg = dark ? RGB(180, 180, 180) : RGB(230, 230, 230);
			HBRUSH h = CreateSolidBrush(BlendColorOnBg(color, alpha, bg));
			if (h) {
				FillRect(hdc, &px, h);
				DeleteObject(h);
			}
		}
	}
	DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);
	BYTE currentAlpha = GetTargetOpacity(s_activeTarget);
	int thumbX = rc.left + (int)(((rc.right - rc.left - 1) * (int)currentAlpha) / 255);
	RECT rcThumb = {thumbX - 1, rc.top - 2, thumbX + 2, rc.bottom + 2};
	HBRUSH hThumb = CreateSolidBrush(RGB(0, 0, 0));
	if (hThumb) {
		FillRect(hdc, &rcThumb, hThumb);
		DeleteObject(hThumb);
	}
}
static BOOL IsOpacityBarHit(HWND hWnd, int x, int y) {
	RECT rc = GetOpacityBarRect(hWnd);
	return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}
static void SetOpacityFromPoint(HWND hWnd, int x, ColorTarget target) {
	RECT rc = GetOpacityBarRect(hWnd);
	int width = rc.right - rc.left;
	if (width <= 0)
		return;
	if (x < rc.left)
		x = rc.left;
	if (x > rc.right - 1)
		x = rc.right - 1;
	BYTE alpha = (width <= 1) ? 255 : (BYTE)(((x - rc.left) * 255) / (width - 1));
	SetTargetOpacity(target, alpha);
	InvalidateRect(hWnd, NULL, FALSE);
}
void ColorboxOnPaint(HWND hWnd) {
	PAINTSTRUCT
	ps;
	HDC hdc;
	RECT rc;
	int i, row, col, x, y;
	int nVertCenter;
	hdc = BeginPaint(hWnd, &ps);
	ClearClientRect(hdc, hWnd, GetSysColorBrush(COLOR_BTNFACE));
	nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
	rc.left = FGBG_OFFSET + 7;
	rc.top = nVertCenter + 7;
	rc.right = rc.left + FGBG_BOX_SIZE;
	rc.bottom = rc.top + FGBG_BOX_SIZE;
	DrawAlphaSwatch(hdc, &rc, Palette_GetSecondaryColor(), Palette_GetSecondaryOpacity());
	DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);
	if (s_activeTarget == COLOR_TARGET_SECONDARY)
		DrawFocusRect(hdc, &rc);
	rc.left = FGBG_OFFSET;
	rc.top = nVertCenter;
	rc.right = rc.left + FGBG_BOX_SIZE;
	rc.bottom = rc.top + FGBG_BOX_SIZE;
	DrawAlphaSwatch(hdc, &rc, Palette_GetPrimaryColor(), Palette_GetPrimaryOpacity());
	DrawEdge(hdc, &rc, BDR_RAISEDINNER, BF_RECT);
	if (s_activeTarget == COLOR_TARGET_PRIMARY)
		DrawFocusRect(hdc, &rc);
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
	if (!hColorboxFont) {
		hColorboxFont = CreateFont(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial");
	}
	HFONT hFont = hColorboxFont ? hColorboxFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT);
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
		DrawText(hdc, labels[i], -1, &rcBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}
	DrawOpacityBar(hdc, hWnd);
	RECT rcOpacityText = GetOpacityBarRect(hWnd);
	rcOpacityText.left = 2;
	rcOpacityText.right = PALETTE_START_X - 4;
	char alphaLabel[24];
	_snprintf(alphaLabel, sizeof(alphaLabel), "A:%3d", (int)GetTargetOpacity(s_activeTarget));
	alphaLabel[sizeof(alphaLabel) - 1] = '\0';
	DrawText(hdc, alphaLabel, -1, &rcOpacityText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	SelectObject(hdc, hOldFont);
	EndPaint(hWnd, &ps);
}
void ColorboxOnLButtonDown(HWND hWnd, int x, int y) {
	if (IsPrimaryBoxHit(x, y)) {
		s_activeTarget = COLOR_TARGET_PRIMARY;
		InvalidateRect(hWnd, NULL, FALSE);
		return;
	}
	if (IsSecondaryBoxHit(x, y)) {
		s_activeTarget = COLOR_TARGET_SECONDARY;
		InvalidateRect(hWnd, NULL, FALSE);
		return;
	}
	if (IsOpacityBarHit(hWnd, x, y)) {
		s_opacityDrag = TRUE;
		SetCapture(hWnd);
		SetOpacityFromPoint(hWnd, x, s_activeTarget);
		return;
	}
	if (x >= BTN_START_X && x <= BTN_START_X + BTN_WIDTH) {
		int btnIdx = (y - 2) / (BTN_HEIGHT + BTN_GAP);
		if (btnIdx >= 0 && btnIdx < 3 && y >= 2 + btnIdx * (BTN_HEIGHT + BTN_GAP) && y <= 2 + btnIdx * (BTN_HEIGHT + BTN_GAP) + BTN_HEIGHT) {
			if (btnIdx == 0)
				memcpy(paletteColors, paletteXP, sizeof(paletteColors));
			else if (btnIdx == 1)
				memcpy(paletteColors, paletteVista, sizeof(paletteColors));
			else
				memcpy(paletteColors, paletteWin7, sizeof(paletteColors));
			InvalidateRect(hWnd, NULL, FALSE);
			return;
		}
	}
	int idx = GetColorIndexAt(x, y);
	if (idx != -1) {
		Palette_SetPrimaryColor(paletteColors[idx]);
		s_activeTarget = COLOR_TARGET_PRIMARY;
		InvalidateRect(hWnd, NULL, FALSE);
	}
}
void ColorboxOnRButtonDown(HWND hWnd, int x, int y) {
	if (IsPrimaryBoxHit(x, y)) {
		s_activeTarget = COLOR_TARGET_PRIMARY;
		InvalidateRect(hWnd, NULL, FALSE);
		return;
	}
	if (IsSecondaryBoxHit(x, y)) {
		s_activeTarget = COLOR_TARGET_SECONDARY;
		InvalidateRect(hWnd, NULL, FALSE);
		return;
	}
	if (IsOpacityBarHit(hWnd, x, y)) {
		s_activeTarget = COLOR_TARGET_SECONDARY;
		s_opacityDrag = TRUE;
		SetCapture(hWnd);
		SetOpacityFromPoint(hWnd, x, s_activeTarget);
		return;
	}
	int idx = GetColorIndexAt(x, y);
	if (idx != -1) {
		Palette_SetSecondaryColor(paletteColors[idx]);
		s_activeTarget = COLOR_TARGET_SECONDARY;
		InvalidateRect(hWnd, NULL, FALSE);
	}
}
void ColorboxOnDoubleClick(HWND hWnd, int x, int y) {
	ColorboxSyncCustomColors();
	if (IsFgBgBoxHit(x, y)) {
		int nVertCenter = (COLORBOX_HEIGHT - (CELL_SIZE * 2 + CELL_GAP)) / 2;
		BOOL bHitFG = (x >= FGBG_OFFSET && x < FGBG_OFFSET + FGBG_BOX_SIZE && y >= nVertCenter && y < nVertCenter + FGBG_BOX_SIZE);
		COLORREF *pColor = bHitFG ? Palette_GetPrimaryColorPtr() : Palette_GetSecondaryColorPtr();
		s_activeTarget = bHitFG ? COLOR_TARGET_PRIMARY : COLOR_TARGET_SECONDARY;
		COLORREF crNew = *pColor;
		if (ChooseColorDialog(hWnd, &crNew)) {
			*pColor = crNew;
			InvalidateRect(hWnd, NULL, FALSE);
		}
	} else {
		int idx = GetColorIndexAt(x, y);
		if (idx != -1) {
			COLORREF crNew = paletteColors[idx];
			if (ChooseColorDialog(hWnd, &crNew)) {
				paletteColors[idx] = crNew;
				Palette_SetPrimaryColor(crNew);
				s_activeTarget = COLOR_TARGET_PRIMARY;
				InvalidateRect(hWnd, NULL, FALSE);
			}
		}
	}
}
void ColorboxOnMouseMove(HWND hWnd, int x, int y, int wParam) {
	(void)wParam;
	if (!s_opacityDrag)
		return;
	SetOpacityFromPoint(hWnd, x, s_activeTarget);
}
void ColorboxOnButtonUp(HWND hWnd) {
	if (!s_opacityDrag)
		return;
	s_opacityDrag = FALSE;
	if (GetCapture() == hWnd)
		ReleaseCapture();
}
LRESULT CALLBACK ColorboxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		ColorboxOnPaint(hwnd);
		return 0;
	case WM_LBUTTONDOWN:
		ColorboxOnLButtonDown(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
		return 0;
	case WM_RBUTTONDOWN:
		ColorboxOnRButtonDown(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam));
		return 0;
	case WM_MOUSEMOVE:
		ColorboxOnMouseMove(hwnd, (short)LOWORD(lParam), (short)HIWORD(lParam), (int)wParam);
		return 0;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
		ColorboxOnButtonUp(hwnd);
		return 0;
	case WM_CAPTURECHANGED:
		s_opacityDrag = FALSE;
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
	WNDCLASS
	wc;
	static char szClassName[] = "PeztoldColorbox";
	ZeroMemory(&wc, sizeof(wc));
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = ColorboxWndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.lpszClassName = szClassName;
	RegisterClass(&wc);
	hColorboxWnd = CreateWindow(szClassName, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, COLORBOX_HEIGHT, hParent, NULL, hInst, NULL);
}
HWND GetColorboxWindow(void) {
	return hColorboxWnd;
}
