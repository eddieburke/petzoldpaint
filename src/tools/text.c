#include "text_tool.h"
#include "text_font.h"
#include "selection_tool.h"
#include "text_toolbar.h"
#include "canvas.h"
#include "draw.h"
#include "geom.h"
#include "helpers.h"
#include "history.h"
#include "layers.h"
#include "overlay.h"
#include "palette.h"
#include "resource.h"
#include "gdi_utils.h"
#include "commit_bar.h"
#include "tool_options/tool_options.h"
#include "interaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <windowsx.h>
#define TEXT_MARGIN       2
#define TEXT_EDIT_INSET   3
#define MIN_TEXTBOX_SIZE  20
#define DEFAULT_TEXTBOX_W 150
#define DEFAULT_TEXTBOX_H 60
static TextFontState s_font = {0};
void TextFont_SetFaceName(const char *f) {
	if (f)
		lstrcpyn(s_font.lf.lfFaceName, f, LF_FACESIZE);
}
void TextFont_SetSize(int s) {
	if (s >= 1 && s <= 999)
		s_font.nSize = s;
}
void TextFont_GetLogFont(LOGFONT *p) {
	if (p)
		memcpy(p, &s_font.lf, sizeof(LOGFONT));
}
void TextFont_Init(void) {
	memset(&s_font, 0, sizeof(TextFontState));
	s_font.nSize = 11;
	lstrcpy(s_font.lf.lfFaceName, "Arial");
	s_font.lf.lfCharSet = DEFAULT_CHARSET;
	s_font.lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
	s_font.lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	s_font.lf.lfQuality = CLEARTYPE_QUALITY;
	s_font.lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
}
void TextFont_Apply(void) {
	if (s_font.hFont)
		DeleteObject(s_font.hFont);
	HDC hdc = GetScreenDC();
	s_font.lf.lfHeight = -MulDiv(s_font.nSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	ReleaseScreenDC(hdc);
	s_font.lf.lfWeight = s_font.bBold ? FW_BOLD : FW_NORMAL;
	s_font.lf.lfItalic = s_font.bItalic;
	s_font.lf.lfUnderline = s_font.bUnderline;
	s_font.lf.lfStrikeOut = s_font.bStrikeout;
	s_font.hFont = CreateFontIndirect(&s_font.lf);
}
typedef enum { TEXT_NONE, TEXT_DRAWING, TEXT_EDITING, TEXT_RESIZING } TextMode;
typedef struct {
	TextMode mode;
	RECT rcBox;
	POINT
	ptDragStart;
	int nHandle;
	RECT rcBoxStart;
	BOOL bOpaque;
	HWND hEdit;
	WNDPROC
	wpOldEdit;
	HFONT
	hEditFont;
} TextState;
static TextState s_text = {0};
void TextToolbar_Show(BOOL bShow);
static void TextToolbar_UpdateButtonStates(void);
static void TextEdit_UpdatePosition(void);
static void OnTextFontChanged(void);
static void TextEdit_Destroy(void);
static char *TextTool_DuplicateCurrentText(void);
static void ApplyTextFont(void) {
	TextFont_Apply();
	if (s_text.hEdit) {
		if (s_text.hEditFont)
			DeleteObject(s_text.hEditFont);
		LOGFONT
		lf;
		TextFont_GetLogFont(&lf);
		lf.lfHeight = (int)(lf.lfHeight * GetZoomScale());
		s_text.hEditFont = CreateFontIndirect(&lf);
		SendMessage(s_text.hEdit, WM_SETFONT, (WPARAM)s_text.hEditFont, TRUE);
	}
}
void TextRender_Blend(BYTE *dst, int dw, int dh, BYTE *src, int sw, int sh, int x, int y, COLORREF color) {
	BYTE r = GetRValue(color);
	BYTE g = GetGValue(color);
	BYTE b = GetBValue(color);
	BYTE colorAlpha = Palette_GetPrimaryOpacity();
	for (int sy = 0; sy < sh; sy++) {
		int dy = y + sy;
		if (dy < 0 || dy >= dh)
			continue;
		for (int sx = 0; sx < sw; sx++) {
			int dx = x + sx;
			if (dx < 0 || dx >= dw)
				continue;
			BYTE cov = src[(sy * sw + sx) * 4 + 2];
			if (cov == 0)
				continue;
			BYTE alpha = ComposeOpacity(cov, colorAlpha);
			if (!IsSelectionActive() || IsPointInSelection(dx, dy)) {
				PixelOps_BlendPixel(r, g, b, alpha, dst + (dy * dw + dx) * 4, 0);
			}
		}
	}
}
void TextRender_ToActive(const char *txt, int len, int tw, int th, int dx, int dy, BYTE *dst) {
	BYTE *tmpBits;
	HBITMAP hTmp = CreateDibSection32(tw, th, &tmpBits);
	if (!hTmp)
		return;
	HDC hdc = CreateCompatibleDC(NULL);
	if (!hdc) {
		DeleteObject(hTmp);
		return;
	}
	SelectObject(hdc, hTmp);
	ZeroMemory(tmpBits, tw * th * 4);
	SelectObject(hdc, s_font.hFont ? s_font.hFont : GetStockObject(DEFAULT_GUI_FONT));
	SetBkMode(hdc, TRANSPARENT);
	SetTextColor(hdc, RGB(255, 255, 255));
	RECT rc = {0, 0, tw, th};
	DrawTextA(hdc, txt, len, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
	TextRender_Blend(dst, Canvas_GetWidth(), Canvas_GetHeight(), tmpBits, tw, th, dx, dy, Palette_GetPrimaryColor());
	DeleteDC(hdc);
	DeleteObject(hTmp);
}
static void CommitText(HWND hParent) {
	if (s_text.mode == TEXT_NONE || !s_text.hEdit)
		return;
	int len = GetWindowTextLengthA(s_text.hEdit);
	if (len > 0) {
		char *buf = malloc(len + 1);
		if (buf) {
			GetWindowTextA(s_text.hEdit, buf, len + 1);
			BYTE *bits = LayersGetActiveColorBits();
			if (bits) {
				if (s_text.bOpaque) {
					DrawRectAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), s_text.rcBox.left, s_text.rcBox.top, s_text.rcBox.right - s_text.rcBox.left, s_text.rcBox.bottom - s_text.rcBox.top, Palette_GetSecondaryColor(), Palette_GetSecondaryOpacity(), LAYER_BLEND_NORMAL);
				}
				int ins = TEXT_MARGIN + TEXT_EDIT_INSET;
				TextRender_ToActive(buf, len, (s_text.rcBox.right - s_text.rcBox.left) - ins * 2, (s_text.rcBox.bottom - s_text.rcBox.top) - ins * 2, s_text.rcBox.left + ins, s_text.rcBox.top + ins, bits);
				Interaction_MarkModified();
				Interaction_Commit("Text Placement");
			}
			free(buf);
		}
	}
	LayersClearDraft();
	TextEdit_Destroy();
	TextToolbar_Show(FALSE);
	s_text.mode = TEXT_NONE;
	Interaction_EndQuiet();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
BOOL TextTool_Cancel(void) {
	if (s_text.mode == TEXT_NONE)
		return FALSE;
	LayersClearDraft();
	TextEdit_Destroy();
	TextToolbar_Show(FALSE);
	s_text.mode = TEXT_NONE;
	Interaction_Abort();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	return TRUE;
}
BOOL IsTextEditing(void) {
	return s_text.mode != TEXT_NONE;
}
BOOL TextTool_TryEditUndo(void) {
	if (s_text.mode != TEXT_EDITING || !s_text.hEdit)
		return FALSE;
	if (GetFocus() != s_text.hEdit)
		return FALSE;
	SendMessage(s_text.hEdit, EM_UNDO, 0, 0);
	return TRUE;
}
void TextTool_Deactivate(void) {
	if (s_text.mode != TEXT_NONE)
		CommitText(NULL);
}
static LRESULT CALLBACK TextEditProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	if (msg == WM_KEYDOWN) {
		if (wp == VK_ESCAPE) {
			TextTool_Cancel();
			return 0;
		}
		if (IsCtrlDown()) {
			switch (wp) {
			case 'B':
				s_font.bBold = !s_font.bBold;
				ApplyTextFont();
				TextToolbar_UpdateButtonStates();
				return 0;
			case 'I':
				s_font.bItalic = !s_font.bItalic;
				ApplyTextFont();
				TextToolbar_UpdateButtonStates();
				return 0;
			case 'U':
				s_font.bUnderline = !s_font.bUnderline;
				ApplyTextFont();
				TextToolbar_UpdateButtonStates();
				return 0;
			}
		}
	} else if (msg == WM_CHAR || msg == WM_KEYUP || msg == WM_PASTE) {
		OnTextFontChanged();
	}
	return CallWindowProc(s_text.wpOldEdit, hwnd, msg, wp, lp);
}
static LRESULT CALLBACK CanvasSubclassText(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR data) {
	if (msg == WM_CTLCOLOREDIT && (HWND)lp == s_text.hEdit && !s_text.bOpaque) {
		SetBkMode((HDC)wp, TRANSPARENT);
		return (LRESULT)GetStockObject(NULL_BRUSH);
	}
	if (msg == WM_RBUTTONUP)
		TextTool_Cancel();
	return DefSubclassProc(hwnd, msg, wp, lp);
}
static void TextEdit_UpdatePosition(void) {
	if (!s_text.hEdit)
		return;
	double scale = GetZoomScale();
	int ox, oy;
	GetCanvasViewportOrigin(&ox, &oy);
	int ins = (int)((TEXT_MARGIN + TEXT_EDIT_INSET) * scale);
	RECT rc = s_text.rcBox;
	NormalizeRect(&rc);
	MoveWindow(s_text.hEdit, ox + (int)(rc.left * scale) + ins, oy + (int)(rc.top * scale) + ins, (int)((rc.right - rc.left) * scale) - ins * 2, (int)((rc.bottom - rc.top) * scale) - ins * 2, TRUE);
	ShowWindow(s_text.hEdit, SW_SHOW);
	SetFocus(s_text.hEdit);
}
static void TextEdit_Destroy(void) {
	if (s_text.hEdit) {
		RemoveWindowSubclass(GetParent(s_text.hEdit), CanvasSubclassText, 1);
		SetWindowLongPtr(s_text.hEdit, GWLP_WNDPROC, (LONG_PTR)s_text.wpOldEdit);
		DestroyWindow(s_text.hEdit);
		s_text.hEdit = NULL;
	}
	if (s_text.hEditFont) {
		DeleteObject(s_text.hEditFont);
		s_text.hEditFont = NULL;
	}
}
static HWND hToolbar = NULL;
static LRESULT CALLBACK TextToolbarWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
	case WM_CREATE: {
		int x = 6, y = 3;
		HFONT hUIFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
		HWND hFontCB = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, x, y, 160, 300, hwnd, (HMENU)IDC_TEXT_FONT, hInst, NULL);
		HWND hSizeCB = CreateWindow("COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | WS_VSCROLL, x + 164, y, 55, 300, hwnd, (HMENU)IDC_TEXT_SIZE, hInst, NULL);
		SendMessage(hFontCB, WM_SETFONT, (WPARAM)hUIFont, FALSE);
		SendMessage(hSizeCB, WM_SETFONT, (WPARAM)hUIFont, FALSE);
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Arial");
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Courier New");
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Georgia");
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Segoe UI");
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Times New Roman");
		SendMessage(hFontCB, CB_ADDSTRING, 0, (LPARAM) "Verdana");
		SendMessage(hFontCB, CB_SELECTSTRING, -1, (LPARAM) "Arial");
		const char *sz[] = {"8", "9", "10", "11", "12", "14", "16", "18", "20", "24", "26", "28", "36", "48", "72"};
		for (int i = 0; i < 15; i++) {
			SendMessage(hSizeCB, CB_ADDSTRING, 0, (LPARAM)sz[i]);
		}
		SendMessage(hSizeCB, CB_SELECTSTRING, -1, (LPARAM) "11");
		int bx = x + 224;
		CreateWindow("BUTTON", "B", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, bx, y, 24, 24, hwnd, (HMENU)IDC_TEXT_BOLD, hInst, NULL);
		CreateWindow("BUTTON", "I", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, bx + 26, y, 24, 24, hwnd, (HMENU)IDC_TEXT_ITALIC, hInst, NULL);
		CreateWindow("BUTTON", "U", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, bx + 52, y, 24, 24, hwnd, (HMENU)IDC_TEXT_UNDERLINE, hInst, NULL);
		CreateWindow("BUTTON", "S", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE, bx + 78, y, 24, 24, hwnd, (HMENU)IDC_TEXT_STRIKEOUT, hInst, NULL);
		return 0;
	}
	case WM_COMMAND: {
		int id = LOWORD(wp), code = HIWORD(wp);
		if (id == IDC_TEXT_FONT && code == CBN_SELCHANGE)
			OnTextFontChanged();
		else if (id == IDC_TEXT_SIZE && (code == CBN_SELCHANGE || code == CBN_EDITCHANGE))
			OnTextFontChanged();
		else if (id == IDC_TEXT_BOLD) {
			s_font.bBold = !s_font.bBold;
			ApplyTextFont();
			TextToolbar_UpdateButtonStates();
		} else if (id == IDC_TEXT_ITALIC) {
			s_font.bItalic = !s_font.bItalic;
			ApplyTextFont();
			TextToolbar_UpdateButtonStates();
		} else if (id == IDC_TEXT_UNDERLINE) {
			s_font.bUnderline = !s_font.bUnderline;
			ApplyTextFont();
			TextToolbar_UpdateButtonStates();
		} else if (id == IDC_TEXT_STRIKEOUT) {
			s_font.bStrikeout = !s_font.bStrikeout;
			ApplyTextFont();
			TextToolbar_UpdateButtonStates();
		}
		return 0;
	}
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}
static void OnTextFontChanged(void) {
	if (hToolbar) {
		char buf[128];
		HWND hFontCB = GetDlgItem(hToolbar, IDC_TEXT_FONT);
		int idx = (int)SendMessage(hFontCB, CB_GETCURSEL, 0, 0);
		if (idx != CB_ERR) {
			SendMessage(hFontCB, CB_GETLBTEXT, idx, (LPARAM)buf);
			TextFont_SetFaceName(buf);
		}
		HWND hSizeCB = GetDlgItem(hToolbar, IDC_TEXT_SIZE);
		GetWindowTextA(hSizeCB, buf, sizeof(buf));
		int s = atoi(buf);
		if (s > 0)
			TextFont_SetSize(s);
	}
	TextFont_Apply();
	ApplyTextFont();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
static void TextTool_HandleMouseDown(HWND hwnd, int x, int y, int btn) {
	(void)btn;
	if (s_text.mode == TEXT_NONE) {
		if (!Interaction_Begin(hwnd, x, y, MK_LBUTTON, TOOL_TEXT))
			return;
		s_text.ptDragStart = (POINT){x, y};
		s_text.rcBox = (RECT){x, y, x, y};
		s_text.mode = TEXT_DRAWING;
	} else if (s_text.mode == TEXT_EDITING) {
		COMMIT_BAR_HANDLE_CLICK(&s_text.rcBox, x, y, CommitText(hwnd), TextTool_Cancel());
		int h = Overlay_HitTestBoxHandles(&s_text.rcBox, x, y);
		if (h >= 0) {
			if (!Interaction_Begin(hwnd, x, y, MK_LBUTTON, TOOL_TEXT))
				return;
			s_text.nHandle = h;
			s_text.ptDragStart = (POINT){x, y};
			s_text.rcBoxStart = s_text.rcBox;
			s_text.mode = TEXT_RESIZING;
		} else if (!PtInRect(&s_text.rcBox, (POINT){x, y}))
			CommitText(hwnd);
	}
}
BOOL TextTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton) {
	if (s_text.mode != TEXT_EDITING || nButton != MK_LBUTTON)
		return FALSE;
	int hit = CommitBar_HitTestScreen(&s_text.rcBox, screenX, screenY);
	if (hit == COMMIT_BAR_HIT_COMMIT) {
		CommitText(hWnd);
		return TRUE;
	}
	if (hit == COMMIT_BAR_HIT_CANCEL) {
		TextTool_Cancel();
		return TRUE;
	}
	return FALSE;
}
static void TextTool_HandleMouseMove(HWND hwnd, int x, int y, int btn) {
	(void)btn;
	if (GetCapture() != hwnd)
		return;
	if (s_text.mode == TEXT_DRAWING) {
		s_text.rcBox.left = min(s_text.ptDragStart.x, x);
		s_text.rcBox.top = min(s_text.ptDragStart.y, y);
		s_text.rcBox.right = max(s_text.ptDragStart.x, x) + 1;
		s_text.rcBox.bottom = max(s_text.ptDragStart.y, y) + 1;
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	} else if (s_text.mode == TEXT_RESIZING) {
		s_text.rcBox = s_text.rcBoxStart;
		if (s_text.nHandle == HT_BODY) {
			OffsetRect(&s_text.rcBox, x - s_text.ptDragStart.x, y - s_text.ptDragStart.y);
		} else {
			ResizeRect(&s_text.rcBox, s_text.nHandle, x - s_text.ptDragStart.x, y - s_text.ptDragStart.y, MIN_TEXTBOX_SIZE, IsShiftDown());
		}
		NormalizeRect(&s_text.rcBox);
		TextEdit_UpdatePosition();
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
}
static void TextTool_HandleMouseUp(HWND hwnd, int x, int y, int btn) {
	(void)btn;
	if (GetCapture() == hwnd)
		ReleaseCapture();
	if (s_text.mode == TEXT_DRAWING) {
		s_text.rcBox.left = min(s_text.ptDragStart.x, x);
		s_text.rcBox.top = min(s_text.ptDragStart.y, y);
		s_text.rcBox.right = max(s_text.ptDragStart.x, x) + 1;
		s_text.rcBox.bottom = max(s_text.ptDragStart.y, y) + 1;
		if ((s_text.rcBox.right - s_text.rcBox.left) < MIN_TEXTBOX_SIZE) {
			s_text.rcBox.right = s_text.rcBox.left + DEFAULT_TEXTBOX_W;
		}
		if ((s_text.rcBox.bottom - s_text.rcBox.top) < MIN_TEXTBOX_SIZE) {
			s_text.rcBox.bottom = s_text.rcBox.top + DEFAULT_TEXTBOX_H;
		}
		s_text.hEdit = CreateWindowEx(0, "EDIT", "", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_LEFT, 0, 0, 0, 0, hwnd, NULL, hInst, NULL);
		if (s_text.hEdit) {
			if (!s_font.hFont)
				TextFont_Init();
			ApplyTextFont();
			s_text.wpOldEdit = (WNDPROC)SetWindowLongPtr(s_text.hEdit, GWLP_WNDPROC, (LONG_PTR)TextEditProc);
			SetWindowSubclass(hwnd, CanvasSubclassText, 1, 0);
			TextEdit_UpdatePosition();
			s_text.mode = TEXT_EDITING;
			TextToolbar_Show(TRUE);
			TextToolbar_UpdateButtonStates();
			HistoryPushSession("Create Text Box");
		}
	} else if (s_text.mode == TEXT_RESIZING) {
		s_text.mode = TEXT_EDITING;
		TextEdit_UpdatePosition();
		HistoryPushSession("Adjust Text Box");
	}
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
void TextTool_OnPointer(const ToolPointerEvent *ev) {
	if (!ev)
		return;
	switch (ev->type) {
	case TOOL_POINTER_DOWN:
		TextTool_HandleMouseDown(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_MOVE:
		TextTool_HandleMouseMove(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	case TOOL_POINTER_UP:
		TextTool_HandleMouseUp(ev->hwnd, ev->bmp.x, ev->bmp.y, (int)ev->buttons);
		break;
	default:
		break;
	}
}
void TextTool_DrawOverlay(const OverlayContext *ctx) {
	if (s_text.mode == TEXT_NONE)
		return;
	Overlay_DrawSelectionFrame(ctx, &s_text.rcBox, (s_text.mode == TEXT_DRAWING));
	if (s_text.mode == TEXT_EDITING || s_text.mode == TEXT_RESIZING)
		Overlay_DrawBoxHandles(ctx, &s_text.rcBox);
	if (s_text.mode == TEXT_EDITING || s_text.mode == TEXT_RESIZING)
		CommitBar_Draw(ctx, &s_text.rcBox);
}
void TextTool_OnViewportChanged(HWND hwnd) {
	(void)hwnd;
	if (s_text.mode == TEXT_EDITING || s_text.mode == TEXT_RESIZING) {
		ApplyTextFont();
		TextEdit_UpdatePosition();
	}
}
static void TextToolbar_UpdateButtonStates(void) {
	if (!hToolbar)
		return;
	CheckDlgButton(hToolbar, IDC_TEXT_BOLD, s_font.bBold ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hToolbar, IDC_TEXT_ITALIC, s_font.bItalic ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hToolbar, IDC_TEXT_UNDERLINE, s_font.bUnderline ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hToolbar, IDC_TEXT_STRIKEOUT, s_font.bStrikeout ? BST_CHECKED : BST_UNCHECKED);
}
void TextToolbar_Show(BOOL bNum) {
	if (bNum) {
		if (!hToolbar) {
			WNDCLASSEX wc = {0};
			wc.cbSize = sizeof(WNDCLASSEX);
			wc.lpfnWndProc = TextToolbarWndProc;
			wc.hInstance = hInst;
			wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
			wc.lpszClassName = "PeztoldTextToolbar";
			wc.hCursor = LoadCursor(NULL, IDC_ARROW);
			RegisterClassEx(&wc);
			hToolbar = CreateWindowEx(WS_EX_TOOLWINDOW, "PeztoldTextToolbar", "Fonts", WS_POPUP | WS_CAPTION | WS_SYSMENU, 100, 100, 430, 80, hMainWnd, NULL, hInst, NULL);
		}
		ShowWindow(hToolbar, SW_SHOWNOACTIVATE);
	} else if (hToolbar) {
		ShowWindow(hToolbar, SW_HIDE);
	}
}
BOOL TextToolbar_IsVisible(void) {
	return hToolbar && IsWindowVisible(hToolbar);
}
static char *TextTool_DuplicateCurrentText(void) {
	if (!s_text.hEdit)
		return NULL;
	int len = GetWindowTextLengthA(s_text.hEdit);
	char *text = (char *)calloc((size_t)len + 1u, 1u);
	if (!text)
		return NULL;
	if (len > 0)
		GetWindowTextA(s_text.hEdit, text, len + 1);
	return text;
}
TextToolSnapshot *TextTool_CreateSnapshot(void) {
	if (s_text.mode == TEXT_NONE)
		return NULL;
	TextToolSnapshot *snapshot = (TextToolSnapshot *)calloc(1, sizeof(TextToolSnapshot));
	if (!snapshot)
		return NULL;
	snapshot->mode = s_text.mode;
	snapshot->rcBox = s_text.rcBox;
	snapshot->bOpaque = s_text.bOpaque;
	snapshot->font = s_font;
	snapshot->font.hFont = NULL;
	snapshot->text = TextTool_DuplicateCurrentText();
	return snapshot;
}
void TextTool_DestroySnapshot(TextToolSnapshot *snapshot) {
	if (!snapshot)
		return;
	free(snapshot->text);
	free(snapshot);
}
void TextTool_ApplySnapshot(const TextToolSnapshot *snapshot) {
	if (Interaction_IsActive())
		Interaction_EndQuiet();
	LayersClearDraft();
	TextEdit_Destroy();
	TextToolbar_Show(FALSE);
	s_text.mode = TEXT_NONE;
	if (!snapshot) {
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return;
	}
	s_text.bOpaque = snapshot->bOpaque;
	s_text.rcBox = snapshot->rcBox;
	s_font = snapshot->font;
	if (s_font.hFont) {
		DeleteObject(s_font.hFont);
		s_font.hFont = NULL;
	}
	TextFont_Apply();
	if (snapshot->mode == TEXT_DRAWING) {
		s_text.mode = TEXT_DRAWING;
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return;
	}
	HWND hCanvas = GetCanvasWindow();
	if (!hCanvas) {
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return;
	}
	s_text.hEdit = CreateWindowEx(0, "EDIT", snapshot->text ? snapshot->text : "", WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_LEFT, 0, 0, 0, 0, hCanvas, NULL, hInst, NULL);
	if (!s_text.hEdit) {
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return;
	}
	s_text.wpOldEdit = (WNDPROC)SetWindowLongPtr(s_text.hEdit, GWLP_WNDPROC, (LONG_PTR)TextEditProc);
	SetWindowSubclass(hCanvas, CanvasSubclassText, 1, 0);
	ApplyTextFont();
	TextEdit_UpdatePosition();
	s_text.mode = TEXT_EDITING;
	TextToolbar_Show(TRUE);
	TextToolbar_UpdateButtonStates();
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}
