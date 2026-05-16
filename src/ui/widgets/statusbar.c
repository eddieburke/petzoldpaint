#include "statusbar.h"
#include "peztold_core.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "canvas.h"
#include <stdio.h>
#include <string.h>
#include <vssym32.h>
static HWND hStatusWnd = NULL;
static HWND hTrackbar = NULL;
static BOOL bVisible = TRUE;
static int nCurrentX = 0;
static int nCurrentY = 0;
static COLORREF crCurrent = CLR_INVALID;
static double currentZoom = 100.0;
LRESULT CALLBACK StatusBarWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_PAINT: {
		PAINTSTRUCT
		ps;
		HDC hdc;
		RECT rcClient;
		char szBuffer[128];
		hdc = BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rcClient);
		ClearClientRect(hdc, hwnd, GetSysColorBrush(COLOR_BTNFACE));
		if (IsThemeEnabled()) {
			DrawThemedBackground(hdc, L"STATUS", SP_PANE, 0, &rcClient);
		} else {
			RECT rcEdge = rcClient;
			rcEdge.bottom = 2;
			FillRect(hdc, &rcEdge, GetSysColorBrush(COLOR_3DSHADOW));
			rcClient.top = 2;
			FillRect(hdc, &rcClient, GetSysColorBrush(COLOR_BTNFACE));
		}
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
		HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
		StringCchPrintf(szBuffer, sizeof(szBuffer), "  Pos: %d, %d", nCurrentX, nCurrentY);
		TextOut(hdc, 4, 4, szBuffer, (int)strlen(szBuffer));
		if (crCurrent == CLR_INVALID) {
			StringCchCopy(szBuffer, sizeof(szBuffer), "  Color: (transparent)                       ");
		} else {
			StringCchPrintf(szBuffer, sizeof(szBuffer), "  Color: #%02X%02X%02X  RGB(%d, %d, %d)", GetRValue(crCurrent), GetGValue(crCurrent), GetBValue(crCurrent), GetRValue(crCurrent), GetGValue(crCurrent), GetBValue(crCurrent));
		}
		TextOut(hdc, 140, 4, szBuffer, (int)strlen(szBuffer));
		RECT rcSwatch = {140 + 260, 4, 140 + 280, 18};
		if (crCurrent == CLR_INVALID) {
			/* No fill — frame only signals transparency. */
		} else {
			HBRUSH hSwatchBr = CreateSolidBrush(crCurrent);
			if (hSwatchBr) {
				FillRect(hdc, &rcSwatch, hSwatchBr);
				DeleteObject(hSwatchBr);
			}
		}
		FrameRect(hdc, &rcSwatch, GetStockObject(BLACK_BRUSH));
		if (hTrackbar) {
			char szZoom[32];
			StringCchPrintf(szZoom, sizeof(szZoom), "%d%%", (int)currentZoom);
			SetTextAlign(hdc, TA_RIGHT);
			TextOut(hdc, rcClient.right - 146, 4, szZoom, (int)strlen(szZoom));
			SetTextAlign(hdc, TA_LEFT);
		}
		SelectObject(hdc, hOldFont);
		EndPaint(hwnd, &ps);
	}
		return 0;
	case WM_SIZE: {
		RECT rcClient;
		GetClientRect(hwnd, &rcClient);
		if (hTrackbar) {
			int trackbarWidth = 140;
			MoveWindow(hTrackbar, rcClient.right - trackbarWidth - 4, 2, trackbarWidth, rcClient.bottom - 4, TRUE);
		}
		return 0;
	}
	case WM_HSCROLL: {
		if ((HWND)lParam == hTrackbar) {
			LRESULT pos = SendMessage(hTrackbar, TBM_GETPOS, 0, 0);
			Canvas_ApplyZoomCentered((double)pos);
		}
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
void CreateStatusBar(HWND hParent) {
	WNDCLASS
	wc;
	static char szClassName[] = "PeztoldStatusBar";
	ZeroMemory(&wc, sizeof(wc));
	wc.style = 0;
	wc.lpfnWndProc = StatusBarWndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszClassName = szClassName;
	RegisterClass(&wc);
	hStatusWnd = CreateWindow(szClassName, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 100, STATUSBAR_HEIGHT, hParent, NULL, hInst, NULL);
	hTrackbar = CreateWindowEx(0, TRACKBAR_CLASS, "", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_BOTTOM, 0, 0, 140, STATUSBAR_HEIGHT - 4, hStatusWnd, (HMENU)1001, hInst, NULL);
	if (hTrackbar) {
		SendMessage(hTrackbar, TBM_SETRANGE, TRUE, MAKELPARAM(12, 3200));
		SendMessage(hTrackbar, TBM_SETTICFREQ, 100, 0);
		SendMessage(hTrackbar, TBM_SETPAGESIZE, 0, 100);
		SendMessage(hTrackbar, TBM_SETPOS, TRUE, 100);
	}
}
HWND GetStatusBarWindow(void) {
	return hStatusWnd;
}
void StatusBarSetCoordinates(int x, int y) {
	nCurrentX = x;
	nCurrentY = y;
	if (hStatusWnd && bVisible) {
		InvalidateRect(hStatusWnd, NULL, FALSE);
	}
}
void StatusBarSetColor(COLORREF color) {
	crCurrent = color;
	if (hStatusWnd && bVisible) {
		InvalidateRect(hStatusWnd, NULL, FALSE);
	}
}
void StatusBarSetVisible(BOOL bShow) {
	bVisible = bShow;
	if (hStatusWnd) {
		ShowWindow(hStatusWnd, bShow ? SW_SHOW : SW_HIDE);
	}
}
BOOL StatusBarIsVisible(void) {
	return bVisible;
}
int StatusBarGetHeight(void) {
	return bVisible ? STATUSBAR_HEIGHT : 0;
}
void StatusBarUpdateZoom(double z) {
	currentZoom = z;
	if (hTrackbar) {
		SendMessage(hTrackbar, TBM_SETPOS, TRUE, (LPARAM)(int)z);
		if (hStatusWnd && bVisible) {
			RECT rcClient;
			GetClientRect(hStatusWnd, &rcClient);
			rcClient.left = rcClient.right - 200;
			InvalidateRect(hStatusWnd, &rcClient, FALSE);
		}
	}
}
