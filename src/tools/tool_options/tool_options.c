#include "tool_options.h"
#include "canvas.h"
#include "peztold_core.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "resource.h"
#include "tools.h"
#include "tools/selection_tool.h"
#include "ui/widgets/toolbar.h"
#include "presets.h"
#include <commctrl.h>
#include <math.h>
#include <stdio.h>
#include <vssym32.h>
#include <windowsx.h>
static HWND hOptionsWnd = NULL;
static HBITMAP hBmpSelection = NULL;
static HBITMAP hBmpSpray = NULL;
static HBITMAP hBmpLineWidths = NULL;
int nSelectionMode = SELECTION_TRANSPARENT;
int nBrushWidth = 1;
int nSprayRadius = 2;
int nShapeDrawType = SHAPE_BORDER_ONLY;
int nHighlighterTransparency = 40;
int nHighlighterBlendMode = 0;
int nHighlighterEdgeSoftness = 50;
int nHighlighterOpacity = 85;
int nHighlighterSizeVariation = 20;
int nHighlighterTexture = 30;
int nCrayonDensity = 45;
int nCrayonTextureIntensity = 60;
int nCrayonSprayAmount = 40;
int nCrayonColorVariation = 50;
int nCrayonBrightnessRange = 35;
int nCrayonSaturationRange = 30;
int nCrayonHueShiftRange = 10;
static int storedLineWidth = 1;
static int storedBrushId = 8;
static int storedEraserId = 1;
void SetStoredLineWidth(int width) {
	storedLineWidth = width;
}
typedef void (*ToolOptionsDrawFn)(HDC, RECT *);
typedef BOOL (*ToolOptionsLButtonDownFn)(HWND, int, int);
typedef BOOL (*ToolOptionsMouseMoveFn)(HWND, int, int);
typedef void (*ToolOptionsActivatedFn)(void);
typedef struct {
	ToolOptionsDrawFn draw;
	ToolOptionsLButtonDownFn onLButtonDown;
	ToolOptionsMouseMoveFn onMouseMove;
	ToolOptionsActivatedFn onToolActivated;
} ToolOptionsPanel;
#define OPTION_BTN_W                  40
#define OPTION_BTN_H                  30
#define OPTION_GAP                    2
#define OPTIONS_MARGIN                4
#define LINE_BTN_W                    44
#define LINE_BTN_H                    10
#define SEL_COMMIT_BTN_W              24
#define SEL_COMMIT_BTN_H              20
#define SEL_COMMIT_GAP                4
#define OPTION_SLIDER_W               40
#define OPTION_SLIDER_IDEAL_H         50
#define OPTION_SLIDER_MIN_H           28
#define OPTION_SLIDER_LABEL_H         22
#define OPTION_SLIDER_COMPACT_LABEL_H 14
#define OPTION_SMALL_BTN_H            14
#define OPTION_PRESET_BTN_H           18
#define OPTION_SECTION_GAP            5
typedef struct {
	const char *label;
	int *value;
	int minValue;
	int maxValue;
	BOOL invert;
	BOOL displayInversePercent;
	RECT rc;
} OptionSlider;
typedef struct {
	int clientW;
	int clientH;
	int sliderX;
	int sliderW;
	int sliderH;
	int sliderLabelH;
	int sliderPitch;
	int btnX;
	int btnW;
	int btnH;
	int blendY;
	int widthY;
	int presetY;
	BOOL showPreset;
} OptionColumnLayout;
static void SelectionCommitBarGetRects(int clientW, int *pBarY, RECT *rcCommit, RECT *rcCancel) {
	int barY = OPTIONS_MARGIN;
	int totalW = SEL_COMMIT_BTN_W * 2 + SEL_COMMIT_GAP;
	int startX = (clientW - totalW) / 2;
	if (startX < 0)
		startX = 0;
	SetRect(rcCommit, startX, barY, startX + SEL_COMMIT_BTN_W, barY + SEL_COMMIT_BTN_H);
	int cancelX = startX + SEL_COMMIT_BTN_W + SEL_COMMIT_GAP;
	SetRect(rcCancel, cancelX, barY, cancelX + SEL_COMMIT_BTN_W, barY + SEL_COMMIT_BTN_H);
	if (pBarY)
		*pBarY = barY;
}
static int ClientWidthFromRect(const RECT *prcClient) {
	int clientW = prcClient->right - prcClient->left;
	return clientW > 0 ? clientW : TOOLBAR_WIDTH;
}
static int ClientHeightFromRect(const RECT *prcClient) {
	int clientH = prcClient->bottom - prcClient->top;
	return clientH > 0 ? clientH : TOOL_OPTIONS_HEIGHT;
}
static int HitTestFixedRows(int px, int py, int x, int y, int w, int rowH, int gap, int count) {
	POINT pt = {px, py};
	for (int i = 0; i < count; i++) {
		RECT rc = {x, y + i * (rowH + gap), x + w, y + i * (rowH + gap) + rowH};
		if (PtInRect(&rc, pt))
			return i;
	}
	return -1;
}
static int HitTestGrid(int px, int py, int x, int y, int cellW, int cellH, int cols, int rows) {
	POINT pt = {px, py};
	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < cols; col++) {
			RECT rc = {x + col * cellW, y + row * cellH, x + (col + 1) * cellW, y + (row + 1) * cellH};
			if (PtInRect(&rc, pt))
				return row * cols + col;
		}
	}
	return -1;
}
static int SliderValueFromY(int y, const RECT *prcSlider, int minValue, int maxValue, BOOL invert) {
	int sliderH = prcSlider->bottom - prcSlider->top;
	if (sliderH <= 0)
		return minValue;
	int relY = y - prcSlider->top;
	if (relY < 0)
		relY = 0;
	if (relY > sliderH)
		relY = sliderH;
	int range = maxValue - minValue;
	if (range <= 0)
		return minValue;
	int scaled = (relY * range) / sliderH;
	return invert ? (maxValue - scaled) : (minValue + scaled);
}
static void DrawVerticalSlider(HDC hdc, const RECT *prcSlider, int value, int minValue, int maxValue, BOOL invert) {
	RECT rcSlider = *prcSlider;
	HBRUSH
	hOldBr;
	HBRUSH hBr = CreateBrushAndSelect(hdc, RGB(245, 245, 245), &hOldBr);
	if (hBr) {
		FillRect(hdc, &rcSlider, hBr);
		RestoreBrush(hdc, hOldBr);
		Gdi_DeleteBrush(hBr);
	}
	DrawEdge(hdc, &rcSlider, BDR_SUNKENOUTER, BF_RECT);
	int range = maxValue - minValue;
	if (range <= 0)
		range = 1;
	int sliderRange = (rcSlider.bottom - rcSlider.top) - 12;
	int position = invert ? (maxValue - value) : (value - minValue);
	if (position < 0)
		position = 0;
	if (position > range)
		position = range;
	int thumbY = rcSlider.top + 6 + (position * sliderRange / range);
	RECT rcThumb = {rcSlider.left + 3, thumbY - 5, rcSlider.right - 3, thumbY + 5};
	if (IsThemeEnabled()) {
		DrawThemedButtonState(hdc, &rcThumb, PBS_NORMAL);
	} else {
		FillRect(hdc, &rcThumb, GetSysColorBrush(COLOR_BTNFACE));
		DrawEdge(hdc, &rcThumb, BDR_RAISED, BF_RECT);
	}
}
static void DrawCenteredText(HDC hdc, int x, int y, int width, const char *text) {
	SIZE sz;
	GetTextExtentPoint32(hdc, text, (int)strlen(text), &sz);
	int textX = x + (width - sz.cx) / 2;
	TextOut(hdc, textX, y, text, (int)strlen(text));
}
static void InitOptionColumnLayout(const RECT *prcClient, int sliderCount, int sliderLabelH, int blendCount, OptionColumnLayout *layout) {
	int controlsNoPreset = OPTION_SECTION_GAP + 5 * (OPTION_SMALL_BTN_H + OPTION_GAP);
	int controlsWithPreset;
	int availableForSliders;
	int minSliderBlock;
	ZeroMemory(layout, sizeof(*layout));
	layout->clientW = ClientWidthFromRect(prcClient);
	layout->clientH = ClientHeightFromRect(prcClient);
	layout->sliderW = OPTION_SLIDER_W;
	layout->sliderLabelH = sliderLabelH;
	layout->btnW = 40;
	layout->btnH = OPTION_SMALL_BTN_H;
	layout->sliderX = (layout->clientW - layout->sliderW) / 2;
	if (layout->sliderX < OPTIONS_MARGIN)
		layout->sliderX = OPTIONS_MARGIN;
	layout->btnX = (layout->clientW - layout->btnW) / 2;
	if (layout->btnX < OPTIONS_MARGIN)
		layout->btnX = OPTIONS_MARGIN;
	if (blendCount > 0)
		controlsNoPreset += blendCount * (OPTION_SMALL_BTN_H + OPTION_GAP) + OPTION_SECTION_GAP;
	controlsWithPreset = controlsNoPreset + OPTION_SECTION_GAP + OPTION_PRESET_BTN_H;
	minSliderBlock = sliderCount * (OPTION_SLIDER_MIN_H + sliderLabelH);
	layout->showPreset = layout->clientH - OPTIONS_MARGIN - controlsWithPreset >= minSliderBlock;
	availableForSliders = layout->clientH - OPTIONS_MARGIN - (layout->showPreset ? controlsWithPreset : controlsNoPreset);
	layout->sliderH = availableForSliders / sliderCount - sliderLabelH;
	if (layout->sliderH > OPTION_SLIDER_IDEAL_H)
		layout->sliderH = OPTION_SLIDER_IDEAL_H;
	if (layout->sliderH < OPTION_SLIDER_MIN_H)
		layout->sliderH = OPTION_SLIDER_MIN_H;
	layout->sliderPitch = layout->sliderH + sliderLabelH;
	layout->blendY = OPTIONS_MARGIN + sliderCount * layout->sliderPitch + OPTION_SECTION_GAP;
	layout->widthY = layout->blendY;
	if (blendCount > 0)
		layout->widthY += blendCount * (OPTION_SMALL_BTN_H + OPTION_GAP) + OPTION_SECTION_GAP;
	layout->presetY = layout->widthY + 5 * (OPTION_SMALL_BTN_H + OPTION_GAP) + OPTION_SECTION_GAP;
	if (layout->showPreset && layout->presetY + OPTION_PRESET_BTN_H > layout->clientH)
		layout->showPreset = FALSE;
}
static void AssignSliderRects(OptionSlider *sliders, int sliderCount, const OptionColumnLayout *layout) {
	int y = OPTIONS_MARGIN;
	for (int i = 0; i < sliderCount; i++) {
		SetRect(&sliders[i].rc, layout->sliderX, y, layout->sliderX + layout->sliderW, y + layout->sliderH);
		y += layout->sliderPitch;
	}
}
static void DrawOptionSliders(HDC hdc, OptionSlider *sliders, int sliderCount, const OptionColumnLayout *layout) {
	for (int i = 0; i < sliderCount; i++) {
		char szVal[16];
		int value = *sliders[i].value;
		DrawVerticalSlider(hdc, &sliders[i].rc, value, sliders[i].minValue, sliders[i].maxValue, sliders[i].invert);
		if (sliders[i].displayInversePercent) {
			int range = sliders[i].maxValue - sliders[i].minValue;
			int percentage = range > 0 ? (sliders[i].maxValue - value) * 100 / range : 0;
			StringCchPrintf(szVal, sizeof(szVal), "%d%%", percentage);
		} else {
			StringCchPrintf(szVal, sizeof(szVal), "%d", value);
		}
		if (layout->sliderLabelH <= OPTION_SLIDER_COMPACT_LABEL_H) {
			char szCompact[24];
			StringCchPrintf(szCompact, sizeof(szCompact), "%s %s", sliders[i].label, szVal);
			DrawCenteredText(hdc, layout->sliderX, sliders[i].rc.bottom + 2, layout->sliderW, szCompact);
		} else {
			DrawCenteredText(hdc, layout->sliderX, sliders[i].rc.bottom + 2, layout->sliderW, szVal);
			DrawCenteredText(hdc, layout->sliderX, sliders[i].rc.bottom + 14, layout->sliderW, sliders[i].label);
		}
	}
}
static BOOL UpdateOptionSliderFromPoint(OptionSlider *sliders, int sliderCount, int x, int y) {
	POINT pt = {x, y};
	for (int i = 0; i < sliderCount; i++) {
		if (PtInRect(&sliders[i].rc, pt)) {
			*sliders[i].value = SliderValueFromY(y, &sliders[i].rc, sliders[i].minValue, sliders[i].maxValue, sliders[i].invert);
			return TRUE;
		}
	}
	return FALSE;
}
static void DrawGlossySelection(HDC hdc, RECT *prc);
void DrawOptionButtonFrame(HDC hdc, RECT *prc, BOOL bSelected) {
	if (IsThemeEnabled()) {
		DrawThemedButtonState(hdc, prc, bSelected ? PBS_PRESSED : PBS_NORMAL);
		return;
	}
	if (bSelected) {
		DrawGlossySelection(hdc, prc);
		DrawEdge(hdc, prc, EDGE_SUNKEN, BF_RECT);
	} else {
		DrawEdge(hdc, prc, BDR_SUNKENOUTER, BF_RECT);
	}
}
static void DrawGlossySelection(HDC hdc, RECT *prc) {
	int w = prc->right - prc->left;
	int h = prc->bottom - prc->top;
	int sheenH = h / 3;
	TRIVERTEX vert[4];
	GRADIENT_RECT gRect[2];
	vert[0].x = prc->left;
	vert[0].y = prc->top + sheenH;
	vert[0].Red = 0x3000;
	vert[0].Green = 0x6000;
	vert[0].Blue = 0xD000;
	vert[0].Alpha = 0x0000;
	vert[1].x = prc->right;
	vert[1].y = prc->bottom;
	vert[1].Red = 0x1000;
	vert[1].Green = 0x2800;
	vert[1].Blue = 0x8000;
	vert[1].Alpha = 0x0000;
	gRect[0].UpperLeft = 0;
	gRect[0].LowerRight = 1;
	GradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_V);
	vert[0].x = prc->left;
	vert[0].y = prc->top;
	vert[0].Red = 0xA000;
	vert[0].Green = 0xD000;
	vert[0].Blue = 0xFF00;
	vert[0].Alpha = 0x0000;
	vert[1].x = prc->right;
	vert[1].y = prc->top + sheenH;
	vert[1].Red = 0x4000;
	vert[1].Green = 0x7800;
	vert[1].Blue = 0xE000;
	vert[1].Alpha = 0x0000;
	GradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_V);
	HPEN hOldPen;
	HPEN hHighlight = CreatePenAndSelect(hdc, PS_SOLID, 1, RGB(100, 140, 200), &hOldPen);
	if (hHighlight) {
		MoveToEx(hdc, prc->left + 1, prc->bottom - 2, NULL);
		LineTo(hdc, prc->right - 1, prc->bottom - 2);
		RestorePen(hdc, hOldPen);
		Gdi_DeletePen(hHighlight);
	}
}
static void DrawSelectionOptions(HDC hdc, RECT *prcClient) {
	int x, y, srcW, srcH;
	RECT rc, rcInner;
	BITMAP
	bm;
	HDC hMemDC;
	HBRUSH
	hBr, hOldBr;
	BOOL bHasSelection = IsSelectionActive();
	if (bHasSelection) {
		int barY;
		RECT rcCommit, rcCancel;
		SelectionCommitBarGetRects(prcClient->right, &barY, &rcCommit, &rcCancel);
		int btnH = SEL_COMMIT_BTN_H;
		int btnW = SEL_COMMIT_BTN_W;
		rc = rcCommit;
		DrawOptionButtonFrame(hdc, &rc, FALSE);
		HPEN hOldPen;
		HPEN hPen = CreatePenAndSelect(hdc, PS_SOLID, 2, RGB(0, 150, 0), &hOldPen);
		MoveToEx(hdc, rc.left + 6, rc.top + btnH / 2, NULL);
		LineTo(hdc, rc.left + 10, rc.top + btnH - 4);
		LineTo(hdc, rc.left + btnW - 6, rc.top + 6);
		RestorePen(hdc, hOldPen);
		Gdi_DeletePen(hPen);
		rc = rcCancel;
		DrawOptionButtonFrame(hdc, &rc, FALSE);
		hPen = CreatePenAndSelect(hdc, PS_SOLID, 2, RGB(200, 0, 0), &hOldPen);
		MoveToEx(hdc, rc.left + 6, rc.top + 4, NULL);
		LineTo(hdc, rc.left + btnW - 6, rc.top + btnH - 4);
		MoveToEx(hdc, rc.left + btnW - 6, rc.top + 4, NULL);
		LineTo(hdc, rc.left + 6, rc.top + btnH - 4);
		RestorePen(hdc, hOldPen);
		Gdi_DeletePen(hPen);
		y = barY + btnH + 8;
	} else {
		y = OPTIONS_MARGIN;
	}
	x = (prcClient->right - OPTION_BTN_W) / 2;
	SetRect(&rc, x, y, x + OPTION_BTN_W, y + OPTION_BTN_H);
	DrawOptionButtonFrame(hdc, &rc, TRUE);
	if (hBmpSelection) {
		GetObject(hBmpSelection, sizeof(bm), &bm);
		srcW = bm.bmWidth / 2;
		srcH = bm.bmHeight;
		hMemDC = CreateTempDC(hdc);
		GdiSelection bmpSel;
		if (hMemDC && Gdi_SelectObject(hMemDC, hBmpSelection, &bmpSel)) {
			TransparentBlt(hdc, x + (OPTION_BTN_W - srcW) / 2, y + (OPTION_BTN_H - srcH) / 2, srcW, srcH, hMemDC, srcW, 0, srcW, srcH, RGB(128, 0, 0));
			Gdi_RestoreSelection(&bmpSel);
		}
		if (hMemDC)
			DeleteTempDC(hMemDC);
	} else {
		hBr = CreateBrushAndSelect(hdc, RGB(200, 200, 200), &hOldBr);
		rcInner = rc;
		InflateRect(&rcInner, -4, -4);
		FillRect(hdc, &rcInner, hBr);
		RestoreBrush(hdc, hOldBr);
		Gdi_DeleteBrush(hBr);
	}
}
static BOOL SelectionOptionsLButtonDown(HWND hwnd, int x, int y) {
	if (IsSelectionActive()) {
		RECT rcClient;
		GetClientRect(hwnd, &rcClient);
		int barY;
		RECT rcCommit, rcCancel;
		SelectionCommitBarGetRects(rcClient.right, &barY, &rcCommit, &rcCancel);
		if (y >= barY && y < barY + SEL_COMMIT_BTN_H) {
			if (x >= rcCommit.left && x < rcCommit.right) {
				CommitSelection();
				InvalidateRect(hwnd, NULL, FALSE);
				InvalidateRect(GetCanvasWindow(), NULL, FALSE);
				return TRUE;
			}
			if (x >= rcCancel.left && x < rcCancel.right) {
				CancelSelection();
				InvalidateRect(hwnd, NULL, FALSE);
				InvalidateRect(GetCanvasWindow(), NULL, FALSE);
				return TRUE;
			}
		}
	}
	(void)hwnd;
	return FALSE;
}
static void DrawLineWidthOptions(HDC hdc, RECT *prcClient) {
	int i, x, y;
	RECT rc;
	int btnH = 14;
	x = (prcClient->right - 40) / 2;
	if (x < 0)
		x = OPTIONS_MARGIN;
	y = prcClient->top + OPTIONS_MARGIN;
	for (i = 0; i < 5; i++) {
		SetRect(&rc, x, y, x + 40, y + btnH);
		BOOL bSelected = (nBrushWidth == (i + 1));
		DrawOptionButtonFrame(hdc, &rc, bSelected);
		int lineWidth = i + 1;
		int cy = (rc.top + rc.bottom) / 2;
		int cx = (rc.left + rc.right) / 2;
		RECT rcLine;
		rcLine.left = x + 4;
		rcLine.right = x + 40 - 4;
		rcLine.top = cy - (lineWidth / 2);
		rcLine.bottom = rcLine.top + lineWidth;
		if (lineWidth == 1)
			rcLine.bottom = rcLine.top + 1;
		HBRUSH hLineBr = bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
		FillRect(hdc, &rcLine, hLineBr);
		y += btnH + OPTION_GAP;
	}
}
static void SprayOptionsGetRects(int clientW, RECT rects[3]) {
	int btnW = 24;
	int btnH = 24;
	int topY = OPTIONS_MARGIN;
	int bottomY = OPTIONS_MARGIN + btnH + 8;
	SetRect(&rects[0], 1, topY, 1 + btnW, topY + btnH);
	SetRect(&rects[1], clientW - btnW - 1, topY, clientW - 1, topY + btnH);
	SetRect(&rects[2], (clientW - btnW) / 2, bottomY, (clientW - btnW) / 2 + btnW, bottomY + btnH);
}
static void DrawSprayOptions(HDC hdc, RECT *prcClient) {
	int clientW = ClientWidthFromRect(prcClient);
	RECT rects[3];
	SprayOptionsGetRects(clientW, rects);
	for (int i = 0; i < 3; i++)
		DrawOptionButtonFrame(hdc, &rects[i], nSprayRadius == i + 1);
	if (!hBmpSpray)
		return;
	BITMAP
	bm;
	GetObject(hBmpSpray, sizeof(bm), &bm);
	int srcW = bm.bmWidth / 3;
	int srcH = bm.bmHeight;
	HDC hMemDC = CreateTempDC(hdc);
	GdiSelection bmpSel;
	if (hMemDC && Gdi_SelectObject(hMemDC, hBmpSpray, &bmpSel)) {
		for (int i = 0; i < 3; i++) {
			int dstX = rects[i].left + ((rects[i].right - rects[i].left) - srcW) / 2;
			int dstY = rects[i].top + ((rects[i].bottom - rects[i].top) - srcH) / 2;
			TransparentBlt(hdc, dstX, dstY, srcW, srcH, hMemDC, i * srcW, 0, srcW, srcH, RGB(128, 0, 0));
		}
		Gdi_RestoreSelection(&bmpSel);
	}
	if (hMemDC)
		DeleteTempDC(hMemDC);
}
static void DrawBrushOptions(HDC hdc, RECT *prcClient) {
	int row, col, i;
	int xBase = 3;
	int yBase = 3;
	int cellW = 13;
	int cellH = 13;
	for (i = 0; i < 12; i++) {
		col = i % 3;
		row = i / 3;
		int x = xBase + col * cellW;
		int y = yBase + row * cellH;
		RECT rc = {x, y, x + cellW, y + cellH};
		BOOL bSelected = (nBrushWidth == (i + 1));
		if (bSelected) {
			HBRUSH hBr = GetSysColorBrush(COLOR_HIGHLIGHT);
			FrameRect(hdc, &rc, hBr);
			InflateRect(&rc, -1, -1);
			FrameRect(hdc, &rc, hBr);
			InflateRect(&rc, 1, 1);
		} else {
			DrawEdge(hdc, &rc, BDR_SUNKENOUTER, BF_RECT);
		}
		int s = (col == 0) ? 4 : (col == 1) ? 7 : 10;
		int cx = x + cellW / 2;
		int cy = y + cellH / 2;
		COLORREF color = bSelected ? RGB(255, 255, 255) : RGB(0, 0, 0);
		if (row == 0) {
			GdiSelection brushSel, penSel;
			HBRUSH hShapeBr = bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
			Gdi_SelectObject(hdc, hShapeBr, &brushSel);
			Gdi_SelectObject(hdc, GetStockObject(NULL_PEN), &penSel);
			Ellipse(hdc, cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s);
			Gdi_RestoreSelection(&penSel);
			Gdi_RestoreSelection(&brushSel);
		} else if (row == 1) {
			GdiSelection brushSel, penSel;
			HBRUSH hShapeBr = bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
			Gdi_SelectObject(hdc, hShapeBr, &brushSel);
			Gdi_SelectObject(hdc, GetStockObject(NULL_PEN), &penSel);
			Rectangle(hdc, cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s);
			Gdi_RestoreSelection(&penSel);
			Gdi_RestoreSelection(&brushSel);
		} else if (row == 2) {
			int w = s / 2;
			HPEN hLine = CreatePen(PS_SOLID, 1, color);
			GdiSelection lineSel;
			if (Gdi_SelectObject(hdc, hLine, &lineSel)) {
				MoveToEx(hdc, cx + w, cy - w, NULL);
				LineTo(hdc, cx - w, cy + w);
				Gdi_RestoreSelection(&lineSel);
			}
			Gdi_DeletePen(hLine);
		} else if (row == 3) {
			int w = s / 2;
			HPEN hLine = CreatePen(PS_SOLID, 1, color);
			GdiSelection lineSel;
			if (Gdi_SelectObject(hdc, hLine, &lineSel)) {
				MoveToEx(hdc, cx - w, cy - w, NULL);
				LineTo(hdc, cx + w, cy + w);
				Gdi_RestoreSelection(&lineSel);
			}
			Gdi_DeletePen(hLine);
		}
	}
}
static void DrawEraserOptions(HDC hdc, RECT *prcClient) {
	int i;
	int xBase = 10;
	int yBase = 6;
	int cellW = 20;
	int cellH = 20;
	for (i = 0; i < 4; i++) {
		int row = i / 2;
		int col = i % 2;
		int x = xBase + col * cellW;
		int y = yBase + row * cellH;
		RECT rc = {x, y, x + cellW, y + cellH};
		BOOL bSelected = (nBrushWidth == (i + 1));
		DrawOptionButtonFrame(hdc, &rc, bSelected);
		int sizes[] = {4, 6, 8, 10};
		int s = sizes[i];
		int cx = (rc.left + rc.right) / 2;
		int cy = (rc.top + rc.bottom) / 2;
		HBRUSH hShapeBr = bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
		RECT rcEraser = {cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s};
		FillRect(hdc, &rcEraser, hShapeBr);
	}
}
static void DrawShapeOptions(HDC hdc, RECT *prcClient) {
	int x = (prcClient->right - OPTION_BTN_W) / 2;
	if (x < OPTIONS_MARGIN)
		x = OPTIONS_MARGIN;
	int y = OPTIONS_MARGIN;
	int i;
	RECT rc;
	int nBtnH = 20;
	for (i = 0; i < 3; i++) {
		SetRect(&rc, x, y, x + OPTION_BTN_W, y + nBtnH);
		BOOL bSelected = (nShapeDrawType == i);
		if (bSelected) {
			FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
			DrawEdge(hdc, &rc, EDGE_SUNKEN, BF_RECT);
		} else {
			DrawOptionButtonFrame(hdc, &rc, FALSE);
		}
		int cx = (rc.left + rc.right) / 2;
		int cy = (rc.top + rc.bottom) / 2;
		int w = 24;
		int h = 12;
		COLORREF penColor = bSelected ? RGB(255, 255, 255) : RGB(0, 0, 0);
		if (i == SHAPE_BORDER_ONLY) {
			HBRUSH hHoldBr = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
			HPEN hOldPen;
			HPEN hPen = CreatePenAndSelect(hdc, PS_SOLID, 1, penColor, &hOldPen);
			if (hPen) {
				Rectangle(hdc, cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
				RestorePen(hdc, hOldPen);
				Gdi_DeletePen(hPen);
			}
			SelectObject(hdc, hHoldBr);
		} else if (i == SHAPE_BORDER_FILL) {
			HBRUSH hFillBr = CreateSolidBrush(RGB(128, 128, 128));
			if (hFillBr) {
				HBRUSH hHoldBr = (HBRUSH)SelectObject(hdc, hFillBr);
				HPEN hOldPen;
				HPEN hPen = CreatePenAndSelect(hdc, PS_SOLID, 1, penColor, &hOldPen);
				if (hPen) {
					Rectangle(hdc, cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
					RestorePen(hdc, hOldPen);
					Gdi_DeletePen(hPen);
				}
				SelectObject(hdc, hHoldBr);
				Gdi_DeleteBrush(hFillBr);
			}
		} else {
			HBRUSH hBr = bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
			RECT rcIcon = {cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2};
			FillRect(hdc, &rcIcon, hBr);
		}
		y += nBtnH + OPTION_GAP;
	}
}
static void DrawZoomOptions(HDC hdc, RECT *prcClient) {
	int x = 5;
	int y = 5;
	int w = 34;
	int h = 64;
	RECT rcBox = {x, y, x + w, y + h};
	DrawEdge(hdc, &rcBox, BDR_SUNKENOUTER, BF_RECT);
	FillRect(hdc, &rcBox, GetStockObject(WHITE_BRUSH));
	if (hBmpLineWidths) {
		BITMAP
		bm;
		GetObject(hBmpLineWidths, sizeof(bm), &bm);
		int wx = x + (w - bm.bmWidth) / 2;
		int wy = y + (h - bm.bmHeight) / 2;
		HDC hMemDC = CreateTempDC(hdc);
		GdiSelection bmpSel;
		if (hMemDC && Gdi_SelectObject(hMemDC, hBmpLineWidths, &bmpSel)) {
			TransparentBlt(hdc, wx, wy, bm.bmWidth, bm.bmHeight, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, RGB(255, 0, 255));
			Gdi_RestoreSelection(&bmpSel);
		}
		if (hMemDC)
			DeleteTempDC(hMemDC);
	}
	int thumbY;
	int pxH = h - 20;
	double val = Canvas_GetZoom();
	if (val < 12.5)
		val = 12.5;
	if (val > 800.0)
		val = 800.0;
	thumbY = y + 10 + (int)((800.0 - val) * pxH / 788.0);
	RECT rcThumb;
	int thumbH = 9;
	rcThumb.left = x + 2;
	rcThumb.right = x + w - 2;
	rcThumb.top = thumbY - thumbH / 2;
	rcThumb.bottom = thumbY + thumbH / 2 + 1;
	if (IsThemeEnabled()) {
		DrawThemedButtonState(hdc, &rcThumb, PBS_NORMAL);
	} else {
		FillRect(hdc, &rcThumb, GetSysColorBrush(COLOR_BTNFACE));
		DrawEdge(hdc, &rcThumb, BDR_SUNKENOUTER, BF_RECT);
	}
}
static void ActivateLineWidthOptions(void) {
	nBrushWidth = storedLineWidth;
}
static void ActivateBrushOptions(void) {
	nBrushWidth = storedBrushId;
}
static void ActivateEraserOptions(void) {
	nBrushWidth = storedEraserId;
}
static BOOL LineWidthOptionsLButtonDown(HWND hwnd, int x, int y) {
	RECT rcClient;
	int nLineBtnH = 14;
	int nBtnW = 40;
	GetClientRect(hwnd, &rcClient);
	int btnX = (ClientWidthFromRect(&rcClient) - nBtnW) / 2;
	if (btnX < 0)
		btnX = OPTIONS_MARGIN;
	int nRow = HitTestFixedRows(x, y, btnX, OPTIONS_MARGIN, nBtnW, nLineBtnH, OPTION_GAP, 5);
	if (nRow >= 0) {
		nBrushWidth = nRow + 1;
		storedLineWidth = nBrushWidth;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL SprayOptionsLButtonDown(HWND hwnd, int x, int y) {
	int idx = -1;
	RECT rcClient;
	RECT rects[3];
	POINT pt = {x, y};
	GetClientRect(hwnd, &rcClient);
	SprayOptionsGetRects(ClientWidthFromRect(&rcClient), rects);
	for (int i = 0; i < 3; i++) {
		if (PtInRect(&rects[i], pt)) {
			idx = i + 1;
			break;
		}
	}
	if (idx != -1) {
		nSprayRadius = idx;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL BrushOptionsLButtonDown(HWND hwnd, int x, int y) {
	int xBase = 3;
	int yBase = 3;
	int cellW = 13;
	int cellH = 13;
	int idx = HitTestGrid(x, y, xBase, yBase, cellW, cellH, 3, 4);
	if (idx >= 0) {
		nBrushWidth = idx + 1;
		storedBrushId = nBrushWidth;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL EraserOptionsLButtonDown(HWND hwnd, int x, int y) {
	int xBase = 10;
	int yBase = 6;
	int cellW = 20;
	int cellH = 20;
	int idx = HitTestGrid(x, y, xBase, yBase, cellW, cellH, 2, 2);
	if (idx >= 0) {
		nBrushWidth = idx + 1;
		storedEraserId = nBrushWidth;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL ZoomOptionsLButtonDown(HWND hwnd, int x, int y) {
	int h = 64;
	int yStart = 5 + 10;
	int pxH = h - 20;
	int newY = y - yStart;
	if (newY < 0)
		newY = 0;
	if (newY > pxH)
		newY = pxH;
	double val = 788.0 - ((double)newY * 788.0 / pxH);
	double newZoom = 12.0 + val;
	if (fabs(newZoom - 100.0) < 10.0)
		newZoom = 100.0;
	if (Canvas_GetZoom() != newZoom) {
		Canvas_SetZoom(newZoom);
		InvalidateRect(hwnd, NULL, FALSE);
		extern HWND hMainWnd;
		SendMessage(GetParent(hwnd), WM_SIZE, 0, 0);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
	SetCapture(hwnd);
	return TRUE;
}
static BOOL ZoomOptionsMouseMove(HWND hwnd, int x, int y) {
	(void)x;
	int h = 64;
	int yStart = 5 + 10;
	int pxH = h - 20;
	int newY = y - yStart;
	if (newY < 0)
		newY = 0;
	if (newY > pxH)
		newY = pxH;
	double val = 788.0 - ((double)newY * 788.0 / pxH);
	double newZoom = 12.0 + val;
	if (fabs(newZoom - 100.0) < 10.0)
		newZoom = 100.0;
	if (Canvas_GetZoom() != newZoom) {
		Canvas_SetZoom(newZoom);
		InvalidateRect(hwnd, NULL, FALSE);
		extern HWND hMainWnd;
		SendMessage(GetParent(hwnd), WM_SIZE, 0, 0);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	}
	return TRUE;
}
static BOOL ShapeWithLineOptionsLButtonDown(HWND hwnd, int x, int y) {
	RECT rcClient;
	int nShapeBtnH = 20;
	int nShapeTotalH = 3 * (nShapeBtnH + OPTION_GAP);
	int yStart = OPTIONS_MARGIN;
	int btnX;
	GetClientRect(hwnd, &rcClient);
	btnX = (ClientWidthFromRect(&rcClient) - OPTION_BTN_W) / 2;
	if (btnX < OPTIONS_MARGIN)
		btnX = OPTIONS_MARGIN;
	int nRow = HitTestFixedRows(x, y, btnX, yStart, OPTION_BTN_W, nShapeBtnH, OPTION_GAP, 3);
	if (nRow >= 0) {
		nShapeDrawType = nRow;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	if (y >= yStart + nShapeTotalH) {
		int nLineYStart = yStart + nShapeTotalH + 8;
		int nLineBtnH = 14;
		int nLineBtnW = 40;
		int lineX = (ClientWidthFromRect(&rcClient) - nLineBtnW) / 2;
		if (lineX < 0)
			lineX = OPTIONS_MARGIN;
		nRow = HitTestFixedRows(x, y, lineX, nLineYStart, nLineBtnW, nLineBtnH, OPTION_GAP, 5);
		if (nRow >= 0) {
			nBrushWidth = nRow + 1;
			storedLineWidth = nBrushWidth;
			InvalidateRect(hwnd, NULL, FALSE);
			InvalidateRect(GetCanvasWindow(), NULL, FALSE);
			return TRUE;
		}
	}
	return FALSE;
}
static void DrawWidthButtons(HDC hdc, int btnX, int y, int btnW, int btnH) {
	for (int i = 0; i < 5; i++) {
		RECT rc = {btnX, y + i * (btnH + OPTION_GAP), btnX + btnW, y + i * (btnH + OPTION_GAP) + btnH};
		BOOL bSelected = (nBrushWidth == (i + 1));
		DrawOptionButtonFrame(hdc, &rc, bSelected);
		int cy = (rc.top + rc.bottom) / 2;
		int lineWidth = i + 1;
		RECT rcLine = {btnX + 4, cy - (lineWidth / 2), btnX + btnW - 4, 0};
		rcLine.bottom = rcLine.top + lineWidth;
		if (lineWidth == 1)
			rcLine.bottom = rcLine.top + 1;
		FillRect(hdc, &rcLine, bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH));
	}
}
static BOOL ApplyWidthHit(HWND hwnd, int row) {
	if (row < 0)
		return FALSE;
	nBrushWidth = row + 1;
	SetStoredLineWidth(nBrushWidth);
	InvalidateRect(hwnd, NULL, FALSE);
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	return TRUE;
}
static void HighlighterOptions_Draw(HDC hdc, RECT *prcClient) {
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"Trans", &nHighlighterTransparency, 0, 255, TRUE, TRUE}, {"Opac", &nHighlighterOpacity, 0, 100, FALSE, FALSE}, {"Edge", &nHighlighterEdgeSoftness, 0, 100, FALSE, FALSE}, {"Size", &nHighlighterSizeVariation, 0, 100, FALSE, FALSE}, {"Tex", &nHighlighterTexture, 0, 100, FALSE, FALSE}};
	InitOptionColumnLayout(prcClient, 5, OPTION_SLIDER_LABEL_H, 3, &layout);
	AssignSliderRects(sliders, 5, &layout);
	SetupTextRender(hdc, RGB(60, 60, 60));
	HFONT hFontSlider = CreateSegoiUIFont(-11, FW_NORMAL);
	GdiSelection fontSel;
	Gdi_SelectObject(hdc, hFontSlider, &fontSel);
	DrawOptionSliders(hdc, sliders, 5, &layout);
	const char *blendNames[] = {"Mult", "Scrn", "Over"};
	for (int i = 0; i < 3; i++) {
		int y = layout.blendY + i * (layout.btnH + OPTION_GAP);
		RECT rc = {layout.btnX, y, layout.btnX + layout.btnW, y + layout.btnH};
		DrawOptionButtonFrame(hdc, &rc, nHighlighterBlendMode == i);
		DrawCenteredText(hdc, layout.btnX, y, layout.btnW, blendNames[i]);
	}
	DrawWidthButtons(hdc, layout.btnX, layout.widthY, layout.btnW, layout.btnH);
	if (layout.showPreset) {
		RECT rcPreset = {layout.btnX, layout.presetY, layout.btnX + layout.btnW, layout.presetY + OPTION_PRESET_BTN_H};
		DrawOptionButtonFrame(hdc, &rcPreset, FALSE);
		HFONT hFontPreset = CreateSegoiUIFont(-10, FW_NORMAL);
		GdiSelection presetFontSel;
		Gdi_SelectObject(hdc, hFontPreset, &presetFontSel);
		DrawCenteredText(hdc, layout.btnX, layout.presetY + 2, layout.btnW, "Presets");
		Gdi_RestoreSelection(&presetFontSel);
		Gdi_DeleteFont(hFontPreset);
	}
	Gdi_RestoreSelection(&fontSel);
	Gdi_DeleteFont(hFontSlider);
}
static BOOL HighlighterOptions_LButtonDown(HWND hwnd, int x, int y) {
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"Trans", &nHighlighterTransparency, 0, 255, TRUE, TRUE}, {"Opac", &nHighlighterOpacity, 0, 100, FALSE, FALSE}, {"Edge", &nHighlighterEdgeSoftness, 0, 100, FALSE, FALSE}, {"Size", &nHighlighterSizeVariation, 0, 100, FALSE, FALSE}, {"Tex", &nHighlighterTexture, 0, 100, FALSE, FALSE}};
	InitOptionColumnLayout(&rcClient, 5, OPTION_SLIDER_LABEL_H, 3, &layout);
	AssignSliderRects(sliders, 5, &layout);
	if (UpdateOptionSliderFromPoint(sliders, 5, x, y)) {
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		SetCapture(hwnd);
		return TRUE;
	}
	int nRow = HitTestFixedRows(x, y, layout.btnX, layout.blendY, layout.btnW, layout.btnH, OPTION_GAP, 3);
	if (nRow >= 0) {
		nHighlighterBlendMode = nRow;
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	nRow = HitTestFixedRows(x, y, layout.btnX, layout.widthY, layout.btnW, layout.btnH, OPTION_GAP, 5);
	if (ApplyWidthHit(hwnd, nRow))
		return TRUE;
	if (layout.showPreset && x >= layout.btnX && x < layout.btnX + layout.btnW && y >= layout.presetY && y < layout.presetY + OPTION_PRESET_BTN_H) {
		int result = Preset_ShowPopupMenu(hwnd, PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, x, y);
		if (result >= 0)
			Preset_Apply(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, result);
		else if (result == -2)
			Preset_SaveCurrent(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER);
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL HighlighterOptions_MouseMove(HWND hwnd, int x, int y) {
	if (GetCapture() != hwnd)
		return FALSE;
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"Trans", &nHighlighterTransparency, 0, 255, TRUE, TRUE}, {"Opac", &nHighlighterOpacity, 0, 100, FALSE, FALSE}, {"Edge", &nHighlighterEdgeSoftness, 0, 100, FALSE, FALSE}, {"Size", &nHighlighterSizeVariation, 0, 100, FALSE, FALSE}, {"Tex", &nHighlighterTexture, 0, 100, FALSE, FALSE}};
	InitOptionColumnLayout(&rcClient, 5, OPTION_SLIDER_LABEL_H, 3, &layout);
	AssignSliderRects(sliders, 5, &layout);
	if (!UpdateOptionSliderFromPoint(sliders, 5, x, y))
		return FALSE;
	InvalidateRect(hwnd, NULL, FALSE);
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	return TRUE;
}
static void CrayonOptions_Draw(HDC hdc, RECT *prcClient) {
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"D", &nCrayonDensity, 0, 100, TRUE, FALSE}, {"T", &nCrayonTextureIntensity, 0, 100, TRUE, FALSE}, {"S", &nCrayonSprayAmount, 0, 100, TRUE, FALSE}, {"V", &nCrayonColorVariation, 0, 100, TRUE, FALSE}, {"B", &nCrayonBrightnessRange, 0, 100, TRUE, FALSE}, {"S", &nCrayonSaturationRange, 0, 100, TRUE, FALSE}, {"H", &nCrayonHueShiftRange, 0, 100, TRUE, FALSE}};
	InitOptionColumnLayout(prcClient, 7, OPTION_SLIDER_COMPACT_LABEL_H, 0, &layout);
	AssignSliderRects(sliders, 7, &layout);
	SetupTextRender(hdc, RGB(60, 60, 60));
	HFONT hFontSlider = CreateSegoiUIFont(-11, FW_NORMAL);
	GdiSelection fontSel;
	Gdi_SelectObject(hdc, hFontSlider, &fontSel);
	DrawOptionSliders(hdc, sliders, 7, &layout);
	DrawWidthButtons(hdc, layout.btnX, layout.widthY, layout.btnW, layout.btnH);
	if (layout.showPreset) {
		RECT rcPreset = {layout.btnX, layout.presetY, layout.btnX + layout.btnW, layout.presetY + OPTION_PRESET_BTN_H};
		DrawOptionButtonFrame(hdc, &rcPreset, FALSE);
		HFONT hFontPreset = CreateSegoiUIFont(-10, FW_NORMAL);
		GdiSelection presetFontSel;
		Gdi_SelectObject(hdc, hFontPreset, &presetFontSel);
		DrawCenteredText(hdc, layout.btnX, layout.presetY + 2, layout.btnW, "Presets");
		Gdi_RestoreSelection(&presetFontSel);
		Gdi_DeleteFont(hFontPreset);
	}
	Gdi_RestoreSelection(&fontSel);
	Gdi_DeleteFont(hFontSlider);
}
static BOOL CrayonOptions_LButtonDown(HWND hwnd, int x, int y) {
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"D", &nCrayonDensity, 0, 100, TRUE, FALSE}, {"T", &nCrayonTextureIntensity, 0, 100, TRUE, FALSE}, {"S", &nCrayonSprayAmount, 0, 100, TRUE, FALSE}, {"V", &nCrayonColorVariation, 0, 100, TRUE, FALSE}, {"B", &nCrayonBrightnessRange, 0, 100, TRUE, FALSE}, {"S", &nCrayonSaturationRange, 0, 100, TRUE, FALSE}, {"H", &nCrayonHueShiftRange, 0, 100, TRUE, FALSE}};
	InitOptionColumnLayout(&rcClient, 7, OPTION_SLIDER_COMPACT_LABEL_H, 0, &layout);
	AssignSliderRects(sliders, 7, &layout);
	if (UpdateOptionSliderFromPoint(sliders, 7, x, y)) {
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		SetCapture(hwnd);
		return TRUE;
	}
	int nRow = HitTestFixedRows(x, y, layout.btnX, layout.widthY, layout.btnW, layout.btnH, OPTION_GAP, 5);
	if (ApplyWidthHit(hwnd, nRow))
		return TRUE;
	if (layout.showPreset && x >= layout.btnX && x < layout.btnX + layout.btnW && y >= layout.presetY && y < layout.presetY + OPTION_PRESET_BTN_H) {
		int result = Preset_ShowPopupMenu(hwnd, PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, x, y);
		if (result >= 0)
			Preset_Apply(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, result);
		else if (result == -2)
			Preset_SaveCurrent(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON);
		InvalidateRect(hwnd, NULL, FALSE);
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		return TRUE;
	}
	return FALSE;
}
static BOOL CrayonOptions_MouseMove(HWND hwnd, int x, int y) {
	if (GetCapture() != hwnd)
		return FALSE;
	RECT rcClient;
	GetClientRect(hwnd, &rcClient);
	OptionColumnLayout layout;
	OptionSlider sliders[] = {{"D", &nCrayonDensity, 0, 100, TRUE, FALSE}, {"T", &nCrayonTextureIntensity, 0, 100, TRUE, FALSE}, {"S", &nCrayonSprayAmount, 0, 100, TRUE, FALSE}, {"V", &nCrayonColorVariation, 0, 100, TRUE, FALSE}, {"B", &nCrayonBrightnessRange, 0, 100, TRUE, FALSE}, {"S", &nCrayonSaturationRange, 0, 100, TRUE, FALSE}, {"H", &nCrayonHueShiftRange, 0, 100, TRUE, FALSE}};
	InitOptionColumnLayout(&rcClient, 7, OPTION_SLIDER_COMPACT_LABEL_H, 0, &layout);
	AssignSliderRects(sliders, 7, &layout);
	if (!UpdateOptionSliderFromPoint(sliders, 7, x, y))
		return FALSE;
	InvalidateRect(hwnd, NULL, FALSE);
	InvalidateRect(GetCanvasWindow(), NULL, FALSE);
	return TRUE;
}
static void DrawShapeWithLineOptions(HDC hdc, RECT *prcClient) {
	DrawShapeOptions(hdc, prcClient);
	RECT rcLineOpts = *prcClient;
	rcLineOpts.top += 3 * (20 + 2) + 8;
	DrawLineWidthOptions(hdc, &rcLineOpts);
}
static const ToolOptionsPanel s_ToolOptionsPanels[NUM_TOOLS] = {[TOOL_FREEFORM] = {DrawSelectionOptions, SelectionOptionsLButtonDown, NULL, NULL},
                                                                [TOOL_SELECT] = {DrawSelectionOptions, SelectionOptionsLButtonDown, NULL, NULL},
                                                                [TOOL_ERASER] = {DrawEraserOptions, EraserOptionsLButtonDown, NULL, ActivateEraserOptions},
                                                                [TOOL_FILL] = {NULL, NULL, NULL, NULL},
                                                                [TOOL_PICK] = {NULL, NULL, NULL, NULL},
                                                                [TOOL_MAGNIFIER] = {DrawZoomOptions, ZoomOptionsLButtonDown, ZoomOptionsMouseMove, NULL},
                                                                [TOOL_PENCIL] = {NULL, NULL, NULL, NULL},
                                                                [TOOL_BRUSH] = {DrawBrushOptions, BrushOptionsLButtonDown, NULL, ActivateBrushOptions},
                                                                [TOOL_AIRBRUSH] = {DrawSprayOptions, SprayOptionsLButtonDown, NULL, NULL},
                                                                [TOOL_TEXT] = {NULL, NULL, NULL, NULL},
                                                                [TOOL_LINE] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_CURVE] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_RECT] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_POLYGON] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_ELLIPSE] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_ROUNDRECT] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_PEN] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
                                                                [TOOL_HIGHLIGHTER] = {HighlighterOptions_Draw, HighlighterOptions_LButtonDown, HighlighterOptions_MouseMove, ActivateLineWidthOptions},
                                                                [TOOL_CRAYON] = {CrayonOptions_Draw, CrayonOptions_LButtonDown, CrayonOptions_MouseMove, ActivateLineWidthOptions}};
static const ToolOptionsPanel *GetCurrentPanel(void) {
	int tool = Tool_GetCurrent();
	if (tool >= 0 && tool < sizeof(s_ToolOptionsPanels) / sizeof(s_ToolOptionsPanels[0])) {
		return &s_ToolOptionsPanels[tool];
	}
	return NULL;
}
LRESULT CALLBACK ToolOptionsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT: {
		PAINTSTRUCT
		ps;
		HDC hdc;
		RECT rcClient;
		hdc = BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rcClient);
		if (rcClient.right > 0 && rcClient.bottom > 0) {
			HDC hMemDC = CreateTempDC(hdc);
			HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
			GdiSelection bmpSel;
			if (hMemDC && hMemBmp && Gdi_SelectObject(hMemDC, hMemBmp, &bmpSel)) {
				ClearClientRect(hMemDC, hwnd, GetSysColorBrush(COLOR_BTNFACE));
				RECT rcLine = {0, 0, rcClient.right, 1};
				FillRect(hMemDC, &rcLine, GetSysColorBrush(COLOR_3DSHADOW));
				const ToolOptionsPanel *p = GetCurrentPanel();
				if (p && p->draw)
					p->draw(hMemDC, &rcClient);
				BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hMemDC, 0, 0, SRCCOPY);
				Gdi_RestoreSelection(&bmpSel);
			}
			if (hMemBmp)
				DeleteObject(hMemBmp);
			if (hMemDC)
				DeleteTempDC(hMemDC);
		}
		EndPaint(hwnd, &ps);
	}
		return 0;
	case WM_LBUTTONDOWN: {
		const ToolOptionsPanel *p = GetCurrentPanel();
		if (p && p->onLButtonDown)
			p->onLButtonDown(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
	}
		return 0;
	case WM_MOUSEMOVE:
		if (GetCapture() == hwnd) {
			const ToolOptionsPanel *p = GetCurrentPanel();
			if (p && p->onMouseMove)
				p->onMouseMove(hwnd, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		}
		return 0;
	case WM_LBUTTONUP:
		if (GetCapture() == hwnd)
			ReleaseCapture();
		return 0;
	case WM_DESTROY:
		SAFE_DELETE_OBJECT(hBmpSelection);
		SAFE_DELETE_OBJECT(hBmpSpray);
		SAFE_DELETE_OBJECT(hBmpLineWidths);
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
void CreateToolOptions(HWND hParent) {
	WNDCLASS wc = {0};
	static char szClassName[] = "PeztoldToolOptions";
	wc.lpfnWndProc = ToolOptionsWndProc;
	wc.hInstance = hInst;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = szClassName;
	RegisterClass(&wc);
	hBmpSelection = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP863));
	hBmpSpray = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP864));
	hBmpLineWidths = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP9248));
	hOptionsWnd = CreateWindow(szClassName, NULL, WS_CHILD | WS_VISIBLE, 0, 0, TOOLBAR_WIDTH, TOOL_OPTIONS_HEIGHT, hParent, NULL, hInst, NULL);
}
HWND GetToolOptionsWindow(void) {
	return hOptionsWnd;
}
void UpdateToolOptions(int nNewTool) {
	if (nNewTool >= 0 && nNewTool < sizeof(s_ToolOptionsPanels) / sizeof(s_ToolOptionsPanels[0])) {
		const ToolOptionsPanel *p = &s_ToolOptionsPanels[nNewTool];
		if (p->onToolActivated)
			p->onToolActivated();
	}
	if (hOptionsWnd)
		InvalidateRect(hOptionsWnd, NULL, FALSE);
}
