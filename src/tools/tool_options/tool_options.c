#include "tool_options.h"
#include "canvas.h"
#include "peztold_core.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "resource.h"
#include "tools.h"
#include "ui/widgets/toolbar.h"
#include "presets.h"
#include <commctrl.h>
#include <math.h>
#include <stdio.h>
#include <vssym32.h>

static HWND hOptionsWnd = NULL;
static HBITMAP hBmpSelection = NULL;
static HBITMAP hBmpSpray = NULL;
static HBITMAP hBmpLineWidths = NULL;
static HBITMAP hBmpZoomThumb = NULL;

int nSelectionMode = SELECTION_TRANSPARENT;
int nBrushWidth = 1;
int nSprayRadius = 2;
int nShapeDrawType = SHAPE_BORDER_ONLY;

// Highlighter options
int nHighlighterTransparency = 40;
int nHighlighterBlendMode = 0;
int nHighlighterEdgeSoftness = 50;
int nHighlighterOpacity = 85;
int nHighlighterSizeVariation = 20;
int nHighlighterTexture = 30;

// Crayon options
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

void SetStoredLineWidth(int width) { storedLineWidth = width; }

typedef void (*ToolOptionsDrawFn)(HDC, RECT *);
typedef BOOL (*ToolOptionsLButtonDownFn)(HWND, int, int);
typedef BOOL (*ToolOptionsMouseMoveFn)(HWND, int);
typedef void (*ToolOptionsActivatedFn)(void);

typedef struct {
  ToolOptionsDrawFn draw;
  ToolOptionsLButtonDownFn onLButtonDown;
  ToolOptionsMouseMoveFn onMouseMove;
  ToolOptionsActivatedFn onToolActivated;
} ToolOptionsPanel;

#define OPTION_BTN_W 40
#define OPTION_BTN_H 30
#define OPTION_GAP 2
#define OPTIONS_MARGIN 4
#define LINE_BTN_W 44
#define LINE_BTN_H 10

#define SEL_COMMIT_BTN_W 24
#define SEL_COMMIT_BTN_H 20
#define SEL_COMMIT_GAP 4

static void SelectionCommitBarGetRects(int clientW, int *pBarY, RECT *rcCommit,
                                       RECT *rcCancel) {
  int barY = OPTIONS_MARGIN;
  int totalW = SEL_COMMIT_BTN_W * 2 + SEL_COMMIT_GAP;
  int startX = (clientW - totalW) / 2;
  if (startX < 0)
    startX = 0;
  SetRect(rcCommit, startX, barY, startX + SEL_COMMIT_BTN_W,
          barY + SEL_COMMIT_BTN_H);
  int cancelX = startX + SEL_COMMIT_BTN_W + SEL_COMMIT_GAP;
  SetRect(rcCancel, cancelX, barY, cancelX + SEL_COMMIT_BTN_W,
          barY + SEL_COMMIT_BTN_H);
  if (pBarY)
    *pBarY = barY;
}


static int SliderValueFromY(int y, const RECT *prcSlider, int minValue,
                            int maxValue, BOOL invert) {
  int sliderH = prcSlider->bottom - prcSlider->top;
  if (sliderH <= 0) return minValue;
  int relY = y - prcSlider->top;
  if (relY < 0) relY = 0;
  if (relY > sliderH) relY = sliderH;

  int range = maxValue - minValue;
  if (range <= 0) return minValue;

  int scaled = (relY * range) / sliderH;
  return invert ? (maxValue - scaled) : (minValue + scaled);
}

static BOOL UpdateSliderValueFromPoint(int x, int y, int sliderX, int sliderStartY,
                                       int sliderW, int sliderH, int sliderCount,
                                       int **valuePtrs, const int *maxVals,
                                       const BOOL *inverts) {
  int currentY = sliderStartY;
  for (int i = 0; i < sliderCount; i++) {
    RECT rcSlider = {sliderX, currentY, sliderX + sliderW, currentY + sliderH};
    if (PtInRect(&rcSlider, (POINT){x, y})) {
      *valuePtrs[i] = SliderValueFromY(y, &rcSlider, 0, maxVals[i], inverts[i]);
      return TRUE;
    }
    currentY += sliderH + 22;
  }
  return FALSE;
}

static void DrawVerticalSlider(HDC hdc, const RECT *prcSlider, int value,
                               int minValue, int maxValue, BOOL invert) {
  RECT rcSlider = *prcSlider;
  HBRUSH hOldBr;
  HBRUSH hBr = CreateBrushAndSelect(hdc, RGB(245, 245, 245), &hOldBr);
  if (hBr) {
    FillRect(hdc, &rcSlider, hBr);
    RestoreBrush(hdc, hOldBr);
    Gdi_DeleteBrush(hBr);
  }
  DrawEdge(hdc, &rcSlider, BDR_SUNKENOUTER, BF_RECT);

  int range = maxValue - minValue;
  if (range <= 0) range = 1;

  int sliderRange = (rcSlider.bottom - rcSlider.top) - 12;
  int position = invert ? (maxValue - value) : (value - minValue);
  if (position < 0) position = 0;
  if (position > range) position = range;

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

// Draw a glossy, skeuomorphic gradient background for selected buttons
static void DrawGlossySelection(HDC hdc, RECT *prc) {
  int w = prc->right - prc->left;
  int h = prc->bottom - prc->top;
  int sheenH = h / 3; // Top portion for the sheen highlight

  // Main gradient: lighter blue at top to darker blue at bottom
  TRIVERTEX vert[4];
  GRADIENT_RECT gRect[2];

  // Bottom section: main blue gradient
  vert[0].x = prc->left;
  vert[0].y = prc->top + sheenH;
  vert[0].Red = 0x3000;   // ~48
  vert[0].Green = 0x6000; // ~96
  vert[0].Blue = 0xD000;  // ~208
  vert[0].Alpha = 0x0000;

  vert[1].x = prc->right;
  vert[1].y = prc->bottom;
  vert[1].Red = 0x1000;   // ~16
  vert[1].Green = 0x2800; // ~40
  vert[1].Blue = 0x8000;  // ~128
  vert[1].Alpha = 0x0000;

  gRect[0].UpperLeft = 0;
  gRect[0].LowerRight = 1;

  GradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_V);

  // Top sheen section: lighter highlight gradient (glassy sheen)
  vert[0].x = prc->left;
  vert[0].y = prc->top;
  vert[0].Red = 0xA000;   // ~160 - bright highlight
  vert[0].Green = 0xD000; // ~208
  vert[0].Blue = 0xFF00;  // ~255
  vert[0].Alpha = 0x0000;

  vert[1].x = prc->right;
  vert[1].y = prc->top + sheenH;
  vert[1].Red = 0x4000;   // ~64
  vert[1].Green = 0x7800; // ~120
  vert[1].Blue = 0xE000;  // ~224
  vert[1].Alpha = 0x0000;

  GradientFill(hdc, vert, 2, gRect, 1, GRADIENT_FILL_RECT_V);

  // Add a subtle bottom highlight line for depth
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
  BITMAP bm;
  HDC hMemDC;
  HBRUSH hBr, hOldBr;

  // Check if selection is active to show commit bar
  BOOL bHasSelection = IsSelectionActive();

  // Draw commit bar at top when selection is active
  if (bHasSelection) {
    int barY;
    RECT rcCommit, rcCancel;
    SelectionCommitBarGetRects(prcClient->right, &barY, &rcCommit, &rcCancel);
    int btnH = SEL_COMMIT_BTN_H;
    int btnW = SEL_COMMIT_BTN_W;

    // Commit button (checkmark)
    rc = rcCommit;
    DrawOptionButtonFrame(hdc, &rc, FALSE);

    // Draw checkmark
    HPEN hOldPen;
    HPEN hPen = CreatePenAndSelect(hdc, PS_SOLID, 2, RGB(0, 150, 0), &hOldPen);
    MoveToEx(hdc, rc.left + 6, rc.top + btnH / 2, NULL);
    LineTo(hdc, rc.left + 10, rc.top + btnH - 4);
    LineTo(hdc, rc.left + btnW - 6, rc.top + 6);
    RestorePen(hdc, hOldPen);
    Gdi_DeletePen(hPen);

    // Cancel button (X)
    rc = rcCancel;
    DrawOptionButtonFrame(hdc, &rc, FALSE);

    // Draw X
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
    SelectObject(hMemDC, hBmpSelection);

    TransparentBlt(hdc, x + (OPTION_BTN_W - srcW) / 2,
                   y + (OPTION_BTN_H - srcH) / 2, srcW, srcH, hMemDC, srcW, 0,
                   srcW, srcH, RGB(128, 0, 0));
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

    HBRUSH hLineBr =
        bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
    FillRect(hdc, &rcLine, hLineBr);

    y += btnH + OPTION_GAP;
  }
}
static void DrawSprayOptions(HDC hdc, RECT *prcClient) {
   HDC hMemDC;

  int clientW = prcClient->right;
  if (clientW == 0)
    clientW = 50;
  int rowGap = 8;
  POINT pts[3];

  pts[0].x = 1;
  pts[0].y = OPTIONS_MARGIN;

  pts[1].x = clientW - 24 - 1;
  pts[1].y = OPTIONS_MARGIN;

  pts[2].x = (clientW - 24) / 2;
  pts[2].y = OPTIONS_MARGIN + 24 + rowGap;

if (!hBmpSpray)
    return;

  BITMAP bm;
  GetObject(hBmpSpray, sizeof(bm), &bm);
  int srcW = bm.bmWidth / 2;
  int srcH = bm.bmHeight;

  int x = (clientW - OPTION_BTN_W) / 2;
  int y = pts[2].y;

  hMemDC = CreateTempDC(hdc);
  SelectObject(hMemDC, hBmpSpray);

  TransparentBlt(hdc, x + (OPTION_BTN_W - srcW) / 2,
                 y + (OPTION_BTN_H - srcH) / 2, srcW, srcH, hMemDC, srcW, 0,
                 srcW, srcH, RGB(128, 0, 0));
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

    // Simple border for small brush cells - no button frame needed
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

    HBRUSH hShapeBr =
        bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
    HBRUSH hOldBr = SelectObject(hdc, hShapeBr);

    HPEN hNull = GetStockObject(NULL_PEN);
    HPEN hOldPen = SelectObject(hdc, hNull);

    if (row == 0) {
      Ellipse(hdc, cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s);
    } else if (row == 1) {
      Rectangle(hdc, cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s);
    } else if (row == 2) {
      SelectObject(hdc, hOldBr);
      SelectObject(hdc, hOldPen);

      int w = s / 2;
      HPEN hLine = CreatePen(PS_SOLID, 1, color);
      SelectObject(hdc, hLine);
      MoveToEx(hdc, cx + w, cy - w, NULL);
      LineTo(hdc, cx - w, cy + w);
      DeleteObject(hLine);

      hOldBr = SelectObject(hdc, hShapeBr);
      hOldPen = SelectObject(hdc, hNull);
    } else if (row == 3) {
      SelectObject(hdc, hOldBr);
      SelectObject(hdc, hOldPen);

      int w = s / 2;
      HPEN hLine = CreatePen(PS_SOLID, 1, color);
      SelectObject(hdc, hLine);
      MoveToEx(hdc, cx - w, cy - w, NULL);
      LineTo(hdc, cx + w, cy + w);
      DeleteObject(hLine);

      hOldBr = SelectObject(hdc, hShapeBr);
      hOldPen = SelectObject(hdc, hNull);
    }

    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
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

    RECT rc = {x, y, x + cellW - 2, y + cellH - 2};
    BOOL bSelected = (nBrushWidth == (i + 1));

    DrawOptionButtonFrame(hdc, &rc, bSelected);

    int sizes[] = {4, 6, 8, 10};
    int s = sizes[i];
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;

    HBRUSH hShapeBr =
        bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
    RECT rcEraser = {cx - s / 2, cy - s / 2, cx - s / 2 + s, cy - s / 2 + s};
    FillRect(hdc, &rcEraser, hShapeBr);
  }
}
static void DrawShapeOptions(HDC hdc, RECT *prcClient) {
  int x = (prcClient->right - OPTION_BTN_W) / 2;
  int y = OPTIONS_MARGIN;
  int i;
  RECT rc;
  int nBtnH = 20;
  for (i = 0; i < 3; i++) {
    SetRect(&rc, x, y, x + OPTION_BTN_W, y + nBtnH);

    BOOL bSelected = (nShapeDrawType == i);

    // For selected options, fill with black background and draw simple border
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
      HBRUSH hBr =
          bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH);
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
    HDC hMemDC = CreateCompatibleDC(hdc);
    SelectObject(hMemDC, hBmpLineWidths);

    BITMAP bm;
    GetObject(hBmpLineWidths, sizeof(bm), &bm);

    int wx = x + (w - bm.bmWidth) / 2;
    int wy = y + (h - bm.bmHeight) / 2;

    TransparentBlt(hdc, wx, wy, bm.bmWidth, bm.bmHeight, hMemDC, 0, 0,
                   bm.bmWidth, bm.bmHeight, RGB(255, 0, 255));

    DeleteTempDC(hMemDC);
  }

  int thumbY;
  int range = 800 - 12;
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
  int nLineBtnH = 14;
  int nRow = (y - OPTIONS_MARGIN) / (nLineBtnH + OPTION_GAP);
  if (nRow >= 0 && nRow < 5) {
    nBrushWidth = nRow + 1;
    storedLineWidth = nBrushWidth;
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }
  return FALSE;
}

static BOOL SprayOptionsLButtonDown(HWND hwnd, int x, int y) {
  int yBase = OPTIONS_MARGIN;
  int rowGap = 8;
  int rowHeight = 24;
  int idx = -1;

  if (y >= yBase && y < yBase + rowHeight) {
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    int mid = rcClient.right / 2;
    if (x < mid)
      idx = 1;
    else
      idx = 2;
  } else if (y >= yBase + rowHeight + rowGap &&
             y < yBase + rowHeight + rowGap + rowHeight) {
    idx = 3;
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

  int col = (x - xBase) / cellW;
  int row = (y - yBase) / cellH;

  if (col >= 0 && col < 3 && row >= 0 && row < 4) {
    int idx = row * 3 + col;
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

  int col = (x - xBase) / cellW;
  int row = (y - yBase) / cellH;

  if (col >= 0 && col < 2 && row >= 0 && row < 2) {
    int idx = row * 2 + col;
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

static BOOL ZoomOptionsMouseMove(HWND hwnd, int y) {
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
  int nShapeBtnH = 20;
  int nShapeTotalH = 3 * (nShapeBtnH + OPTION_GAP);
  int yStart = OPTIONS_MARGIN;

  if (y < yStart + nShapeTotalH) {
    int nRow = (y - yStart) / (nShapeBtnH + OPTION_GAP);
    if (nRow >= 0 && nRow < 3) {
      nShapeDrawType = nRow;
      InvalidateRect(hwnd, NULL, FALSE);
      InvalidateRect(GetCanvasWindow(), NULL, FALSE);
      return TRUE;
    }
  } else {
    int nLineYStart = yStart + nShapeTotalH + 8;
    int nLineBtnH = 14;
    int nLocalY = y - nLineYStart;

    if (nLocalY >= 0) {
      int nRow = nLocalY / (nLineBtnH + OPTION_GAP);
      if (nRow >= 0 && nRow < 5) {
        nBrushWidth = nRow + 1;
        storedLineWidth = nBrushWidth;
        InvalidateRect(hwnd, NULL, FALSE);
        InvalidateRect(GetCanvasWindow(), NULL, FALSE);
        return TRUE;
      }
    }
  }
  return FALSE;
}


static void HighlighterOptions_Draw(HDC hdc, RECT *prcClient) {
  int clientW = prcClient->right - prcClient->left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, x = (clientW - sliderW) / 2;
  if (x < OPTIONS_MARGIN) x = OPTIONS_MARGIN;
  int y = OPTIONS_MARGIN;

  SetupTextRender(hdc, RGB(60, 60, 60));
  HFONT hFontSlider = CreateSegoiUIFont(-11, FW_NORMAL);
  HFONT hOldFont = SelectObject(hdc, hFontSlider);

  int values[] = {nHighlighterTransparency, nHighlighterOpacity,
                  nHighlighterEdgeSoftness, nHighlighterSizeVariation,
                  nHighlighterTexture};
  const char *labels[] = {"Trans", "Opac", "Edge", "Size", "Tex"};
  BOOL inverts[] = {TRUE, FALSE, FALSE, FALSE, FALSE};
  int maxVals[] = {255, 100, 100, 100, 100};

  for (int i = 0; i < 5; i++) {
    RECT rcSlider = {x, y, x + sliderW, y + sliderH};
    DrawVerticalSlider(hdc, &rcSlider, values[i], 0, maxVals[i], inverts[i]);
    char szVal[8];
    if (i == 0) {
      int percentage = (int)((255 - values[i]) * 100 / 255);
      StringCchPrintf(szVal, sizeof(szVal), "%d%%", percentage);
    } else {
      StringCchPrintf(szVal, sizeof(szVal), "%d", values[i]);
    }
    DrawCenteredText(hdc, x, y + sliderH + 2, sliderW, szVal);
    DrawCenteredText(hdc, x, y + sliderH + 14, sliderW, labels[i]);
    y += sliderH + 22;
  }

  y += 5;
  int btnH = 14, btnW = 40, btnX = (clientW - btnW) / 2;
  if (btnX < OPTIONS_MARGIN) btnX = OPTIONS_MARGIN;

  const char *blendNames[] = {"Mult", "Scrn", "Over"};
  for (int i = 0; i < 3; i++) {
    RECT rc = {btnX, y, btnX + btnW, y + btnH};
    DrawOptionButtonFrame(hdc, &rc, nHighlighterBlendMode == i);
    DrawCenteredText(hdc, btnX, y, btnW, blendNames[i]);
    y += btnH + OPTION_GAP;
  }

  y += 5;
  for (int i = 0; i < 5; i++) {
    RECT rc = {btnX, y, btnX + btnW, y + btnH};
    BOOL bSelected = (nBrushWidth == (i + 1));
    DrawOptionButtonFrame(hdc, &rc, bSelected);
    int cy = (rc.top + rc.bottom) / 2;
    int lineWidth = i + 1;
    RECT rcLine = {btnX + 4, cy - (lineWidth / 2), btnX + btnW - 4, 0};
    rcLine.bottom = rcLine.top + lineWidth;
    if (lineWidth == 1) rcLine.bottom = rcLine.top + 1;
    FillRect(hdc, &rcLine, bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH));
    y += btnH + OPTION_GAP;
  }

  y += 5;
  RECT rcPreset = {btnX, y, btnX + btnW, y + 18};
  DrawOptionButtonFrame(hdc, &rcPreset, FALSE);
  HFONT hFontPreset = CreateSegoiUIFont(-10, FW_NORMAL);
  SelectObject(hdc, hFontPreset);
  DrawCenteredText(hdc, btnX, y + 2, btnW, "Presets");

  SelectObject(hdc, hOldFont);
  Gdi_DeleteFont(hFontSlider);
  Gdi_DeleteFont(hFontPreset);
}

static BOOL HighlighterOptions_LButtonDown(HWND hwnd, int x, int y) {
  RECT rcClient; GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, sliderX = (clientW - sliderW) / 2;
  if (sliderX < OPTIONS_MARGIN) sliderX = OPTIONS_MARGIN;
  int *valuePtrs[] = {&nHighlighterTransparency, &nHighlighterOpacity,
                      &nHighlighterEdgeSoftness, &nHighlighterSizeVariation,
                      &nHighlighterTexture};
  BOOL inverts[] = {TRUE, FALSE, FALSE, FALSE, FALSE};
  int maxVals[] = {255, 100, 100, 100, 100};
  if (UpdateSliderValueFromPoint(x, y, sliderX, OPTIONS_MARGIN, sliderW, sliderH, 5,
                                 valuePtrs, maxVals, inverts)) {
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    SetCapture(hwnd);
    return TRUE;
  }

  int btnH = 14, btnW = 40, btnX = (clientW - btnW) / 2;
  if (btnX < OPTIONS_MARGIN) btnX = OPTIONS_MARGIN;
  int blendY = OPTIONS_MARGIN + 5 * (sliderH + 22) + 5;
  int nRow = (y - blendY) / (btnH + OPTION_GAP);
  if (nRow >= 0 && nRow < 3) {
    nHighlighterBlendMode = nRow;
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }

  int widthY = blendY + 3 * (btnH + OPTION_GAP) + 5;
  nRow = (y - widthY) / (btnH + OPTION_GAP);
  if (nRow >= 0 && nRow < 5) {
    nBrushWidth = nRow + 1;
    SetStoredLineWidth(nBrushWidth);
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }

  int presetY = widthY + 5 * (btnH + OPTION_GAP) + 5;
  if (x >= btnX && x < btnX + btnW && y >= presetY && y < presetY + 18) {
    int result = Preset_ShowPopupMenu(hwnd, PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, x, y);
    if (result >= 0) Preset_Apply(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, result);
    else if (result == -2) Preset_SaveCurrent(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER);
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }
  return FALSE;
}

static BOOL HighlighterOptions_MouseMove(HWND hwnd, int y) {
  if (GetCapture() != hwnd) return FALSE;
  RECT rcClient; GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, sliderX = (clientW - sliderW) / 2;
  if (sliderX < OPTIONS_MARGIN) sliderX = OPTIONS_MARGIN;
  int *valuePtrs[] = {&nHighlighterTransparency, &nHighlighterOpacity,
                      &nHighlighterEdgeSoftness, &nHighlighterSizeVariation,
                      &nHighlighterTexture};
  BOOL inverts[] = {TRUE, FALSE, FALSE, FALSE, FALSE};
  int maxVals[] = {255, 100, 100, 100, 100};

  POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
  if (!UpdateSliderValueFromPoint(pt.x, pt.y, sliderX, OPTIONS_MARGIN, sliderW, sliderH,
                                  5, valuePtrs, maxVals, inverts)) {
    return FALSE;
  }
  InvalidateRect(hwnd, NULL, FALSE);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
  return TRUE;
}


static void CrayonOptions_Draw(HDC hdc, RECT *prcClient) {
  int clientW = prcClient->right - prcClient->left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, x = (clientW - sliderW) / 2;
  if (x < OPTIONS_MARGIN) x = OPTIONS_MARGIN;
  int y = OPTIONS_MARGIN;

  SetupTextRender(hdc, RGB(60, 60, 60));
  HFONT hFontSlider = CreateSegoiUIFont(-11, FW_NORMAL);
  HFONT hOldFont = SelectObject(hdc, hFontSlider);

  int values[] = {nCrayonDensity, nCrayonTextureIntensity,
                  nCrayonSprayAmount, nCrayonColorVariation,
                  nCrayonBrightnessRange, nCrayonSaturationRange,
                  nCrayonHueShiftRange};
  const char *labels[] = {"D", "T", "S", "V", "B", "S", "H"};

  for (int i = 0; i < 7; i++) {
    RECT rcSlider = {x, y, x + sliderW, y + sliderH};
    DrawVerticalSlider(hdc, &rcSlider, values[i], 0, 100, TRUE);
    char szVal[8]; StringCchPrintf(szVal, sizeof(szVal), "%d", values[i]);
    DrawCenteredText(hdc, x, y + sliderH + 2, sliderW, szVal);
    DrawCenteredText(hdc, x, y + sliderH + 14, sliderW, labels[i]);
    y += sliderH + 22;
  }

  y += 5;
  int btnH = 14, btnW = 40, btnX = (clientW - btnW) / 2;
  if (btnX < OPTIONS_MARGIN) btnX = OPTIONS_MARGIN;

  for (int i = 0; i < 5; i++) {
    RECT rc = {btnX, y, btnX + btnW, y + btnH};
    BOOL bSelected = (nBrushWidth == (i + 1));
    DrawOptionButtonFrame(hdc, &rc, bSelected);
    int cy = (rc.top + rc.bottom) / 2;
    int lineWidth = i + 1;
    RECT rcLine = {btnX + 4, cy - (lineWidth / 2), btnX + btnW - 4, 0};
    rcLine.bottom = rcLine.top + lineWidth;
    if (lineWidth == 1) rcLine.bottom = rcLine.top + 1;
    FillRect(hdc, &rcLine, bSelected ? GetStockObject(WHITE_BRUSH) : GetStockObject(BLACK_BRUSH));
    y += btnH + OPTION_GAP;
  }

  y += 5;
  RECT rcPreset = {btnX, y, btnX + btnW, y + 18};
  DrawOptionButtonFrame(hdc, &rcPreset, FALSE);
  HFONT hFontPreset = CreateSegoiUIFont(-10, FW_NORMAL);
  SelectObject(hdc, hFontPreset);
  DrawCenteredText(hdc, btnX, y + 2, btnW, "Presets");

  SelectObject(hdc, hOldFont);
  Gdi_DeleteFont(hFontSlider);
  Gdi_DeleteFont(hFontPreset);
}

static BOOL CrayonOptions_LButtonDown(HWND hwnd, int x, int y) {
  RECT rcClient; GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, sliderX = (clientW - sliderW) / 2;
  if (sliderX < OPTIONS_MARGIN) sliderX = OPTIONS_MARGIN;
  int *valuePtrs[] = {&nCrayonDensity, &nCrayonTextureIntensity,
                      &nCrayonSprayAmount, &nCrayonColorVariation,
                      &nCrayonBrightnessRange, &nCrayonSaturationRange,
                      &nCrayonHueShiftRange};
  static const int kCrayonMaxVals[] = {100, 100, 100, 100, 100, 100, 100};
  static const BOOL kCrayonInverts[] = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE};
  if (UpdateSliderValueFromPoint(x, y, sliderX, OPTIONS_MARGIN, sliderW, sliderH, 7,
                                 valuePtrs, kCrayonMaxVals, kCrayonInverts)) {
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    SetCapture(hwnd);
    return TRUE;
  }

  int btnH = 14, btnW = 40, btnX = (clientW - btnW) / 2;
  if (btnX < OPTIONS_MARGIN) btnX = OPTIONS_MARGIN;
  int widthY = OPTIONS_MARGIN + 7 * (sliderH + 22) + 5;
  int nRow = (y - widthY) / (btnH + OPTION_GAP);
  if (nRow >= 0 && nRow < 5) {
    nBrushWidth = nRow + 1;
    SetStoredLineWidth(nBrushWidth);
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }

  int presetY = widthY + 5 * (btnH + OPTION_GAP) + 5;
  if (x >= btnX && x < btnX + btnW && y >= presetY && y < presetY + 18) {
    int result = Preset_ShowPopupMenu(hwnd, PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, x, y);
    if (result >= 0) Preset_Apply(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON, result);
    else if (result == -2) Preset_SaveCurrent(PRESET_CAT_BRUSH, PRESET_SLOT_CRAYON);
    InvalidateRect(hwnd, NULL, FALSE);
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
    return TRUE;
  }
  return FALSE;
}

static BOOL CrayonOptions_MouseMove(HWND hwnd, int y) {
  if (GetCapture() != hwnd) return FALSE;
  RECT rcClient; GetClientRect(hwnd, &rcClient);
  int clientW = rcClient.right - rcClient.left;
  if (clientW == 0) clientW = TOOLBAR_WIDTH;

  int sliderW = 40, sliderH = 50, sliderX = (clientW - sliderW) / 2;
  if (sliderX < OPTIONS_MARGIN) sliderX = OPTIONS_MARGIN;
  int *valuePtrs[] = {&nCrayonDensity, &nCrayonTextureIntensity,
                      &nCrayonSprayAmount, &nCrayonColorVariation,
                      &nCrayonBrightnessRange, &nCrayonSaturationRange,
                      &nCrayonHueShiftRange};
  static const int kCrayonMaxVals[] = {100, 100, 100, 100, 100, 100, 100};
  static const BOOL kCrayonInverts[] = {TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE};

  POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
  if (!UpdateSliderValueFromPoint(pt.x, pt.y, sliderX, OPTIONS_MARGIN, sliderW, sliderH,
                                  7, valuePtrs, kCrayonMaxVals, kCrayonInverts)) {
    return FALSE;
  }
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

static const ToolOptionsPanel s_ToolOptionsPanels[] = {
    [TOOL_FREEFORM] = {DrawSelectionOptions, SelectionOptionsLButtonDown, NULL, NULL},
    [TOOL_SELECT] = {DrawSelectionOptions, SelectionOptionsLButtonDown, NULL, NULL},
    [TOOL_LINE] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_CURVE] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_PEN] = {DrawLineWidthOptions, LineWidthOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_HIGHLIGHTER] = {HighlighterOptions_Draw, HighlighterOptions_LButtonDown, HighlighterOptions_MouseMove, ActivateLineWidthOptions},
    [TOOL_CRAYON] = {CrayonOptions_Draw, CrayonOptions_LButtonDown, CrayonOptions_MouseMove, ActivateLineWidthOptions},
    [TOOL_RECT] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_ELLIPSE] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_ROUNDRECT] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_POLYGON] = {DrawShapeWithLineOptions, ShapeWithLineOptionsLButtonDown, NULL, ActivateLineWidthOptions},
    [TOOL_AIRBRUSH] = {DrawSprayOptions, SprayOptionsLButtonDown, NULL, NULL},
    [TOOL_MAGNIFIER] = {DrawZoomOptions, ZoomOptionsLButtonDown, ZoomOptionsMouseMove, NULL},
    [TOOL_BRUSH] = {DrawBrushOptions, BrushOptionsLButtonDown, NULL, ActivateBrushOptions},
    [TOOL_ERASER] = {DrawEraserOptions, EraserOptionsLButtonDown, NULL, ActivateEraserOptions}};

static const ToolOptionsPanel *GetCurrentPanel(void) {
  int tool = Tool_GetCurrent();
  if (tool >= 0 && tool < sizeof(s_ToolOptionsPanels) / sizeof(s_ToolOptionsPanels[0])) {
    return &s_ToolOptionsPanels[tool];
  }
  return NULL;
}

LRESULT CALLBACK ToolOptionsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_ERASEBKGND: return 1;
  case WM_PAINT: {
    PAINTSTRUCT ps; HDC hdc; RECT rcClient;
    hdc = BeginPaint(hwnd, &ps);
    GetClientRect(hwnd, &rcClient);
    if (rcClient.right > 0 && rcClient.bottom > 0) {
      HDC hMemDC = CreateCompatibleDC(hdc);
      HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
      HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);
      ClearClientRect(hMemDC, hwnd, GetSysColorBrush(COLOR_BTNFACE));
      RECT rcLine = {0, 0, rcClient.right, 1};
      FillRect(hMemDC, &rcLine, GetSysColorBrush(COLOR_3DSHADOW));
      const ToolOptionsPanel *p = GetCurrentPanel();
      if (p && p->draw) p->draw(hMemDC, &rcClient);
      BitBlt(hdc, 0, 0, rcClient.right, rcClient.bottom, hMemDC, 0, 0, SRCCOPY);
      SelectObject(hMemDC, hOldBmp); DeleteObject(hMemBmp); DeleteTempDC(hMemDC);
    }
    EndPaint(hwnd, &ps);
  } return 0;

  case WM_LBUTTONDOWN: {
    const ToolOptionsPanel *p = GetCurrentPanel();
    if (p && p->onLButtonDown) p->onLButtonDown(hwnd, LOWORD(lParam), HIWORD(lParam));
  } return 0;

  case WM_MOUSEMOVE:
    if (GetCapture() == hwnd) {
      const ToolOptionsPanel *p = GetCurrentPanel();
      if (p && p->onMouseMove) p->onMouseMove(hwnd, HIWORD(lParam));
    } return 0;

  case WM_LBUTTONUP: if (GetCapture() == hwnd) ReleaseCapture(); return 0;
  case WM_DESTROY:
    SAFE_DELETE_OBJECT(hBmpSelection); SAFE_DELETE_OBJECT(hBmpSpray);
    SAFE_DELETE_OBJECT(hBmpLineWidths); SAFE_DELETE_OBJECT(hBmpZoomThumb);
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
  hBmpZoomThumb = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP3550));

  hOptionsWnd = CreateWindow(szClassName, NULL, WS_CHILD | WS_VISIBLE, 0, 0,
                             TOOLBAR_WIDTH, TOOL_OPTIONS_HEIGHT, hParent, NULL,
                             hInst, NULL);
}

HWND GetToolOptionsWindow(void) { return hOptionsWnd; }

void UpdateToolOptions(int nNewTool) {
  if (nNewTool >= 0 && nNewTool < sizeof(s_ToolOptionsPanels) / sizeof(s_ToolOptionsPanels[0])) {
    const ToolOptionsPanel *p = &s_ToolOptionsPanels[nNewTool];
    if (p->onToolActivated) p->onToolActivated();
  }
  if (hOptionsWnd) InvalidateRect(hOptionsWnd, NULL, FALSE);
}

int GetToolOptionsHeight(void) { return TOOL_OPTIONS_HEIGHT; }
