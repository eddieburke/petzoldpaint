/*------------------------------------------------------------
    draw.c - Handle drawing, themed UI, and pixel primitives
------------------------------------------------------------*/

#include "draw.h"
#include "gdi_utils.h"
#include "geom.h"
#include "tools/selection_tool.h"
#include <math.h>
#include <uxtheme.h>
#include <vssym32.h>
#pragma comment(lib, "uxtheme.lib")

void DrawSelectionFrame(HDC hdc, RECT *rc, BOOL bDotted) {
  RECT rcTemp = *rc;
  NormalizeRect(&rcTemp);
  rc = &rcTemp;

  HBRUSH hNull = (HBRUSH)GetStockObject(NULL_BRUSH);
  HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hNull);
  HPEN hOldPen;

  HPEN hWhitePen = CreatePenAndSelect(hdc, PS_SOLID, 1, RGB(255, 255, 255), &hOldPen);
  if (hWhitePen) {
    Rectangle(hdc, rc->left, rc->top, rc->right, rc->bottom);
    RestorePen(hdc, hOldPen);
    Gdi_DeletePen(hWhitePen);
  }

  HPEN hBlackPen = CreatePenAndSelect(hdc, bDotted ? PS_DOT : PS_DASH, 1, RGB(0, 0, 0), &hOldPen);
  if (hBlackPen) {
    Rectangle(hdc, rc->left, rc->top, rc->right, rc->bottom);
    RestorePen(hdc, hOldPen);
    Gdi_DeletePen(hBlackPen);
  }

  SelectObject(hdc, hOldBr);
}

/* HitTestBoxHandlesEx removed - use HitTestBoxHandles from geom.h */

void ResizeRect(RECT *rc, int handle, int dx, int dy, int minSize,
                BOOL bKeepAspect) {
  int origW = rc->right - rc->left;
  int origH = rc->bottom - rc->top;

  if (bKeepAspect && (handle == HT_TL || handle == HT_TR || handle == HT_BR ||
                      handle == HT_BL)) {
    if (origW != 0 && origH != 0) {
      double ratio = fabs((double)origW / (double)origH);
      int adx = abs(dx);
      int ady = abs(dy);

      if ((double)adx / fabs((double)origW) > (double)ady / fabs((double)origH)) {
        dy = (int)(dx / ratio);
        if (handle == HT_TR || handle == HT_BL)
          dy = -dy;
      } else {
        dx = (int)(dy * ratio);
        if (handle == HT_TR || handle == HT_BL)
          dx = -dx;
      }
    }
  }

  if (handle == HT_TL || handle == HT_L || handle == HT_BL)
    rc->left += dx;
  if (handle == HT_TL || handle == HT_T || handle == HT_TR)
    rc->top += dy;
  if (handle == HT_TR || handle == HT_R || handle == HT_BR)
    rc->right += dx;
  if (handle == HT_BL || handle == HT_B || handle == HT_BR)
    rc->bottom += dy;
}

LPCTSTR GetHandleCursor(int handleId) {
  switch (handleId) {
  case HT_TL:
  case HT_BR:
    return IDC_SIZENWSE;
  case HT_TR:
  case HT_BL:
    return IDC_SIZENESW;
  case HT_T:
  case HT_B:
    return IDC_SIZENS;
  case HT_L:
  case HT_R:
    return IDC_SIZEWE;
  case HT_BODY:
    return IDC_SIZEALL;
  default:
    return IDC_ARROW;
  }
}

/*------------------------------------------------------------
    Themed Drawing
------------------------------------------------------------*/

BOOL IsThemeEnabled(void) { return IsThemeActive() && IsAppThemed(); }

BOOL DrawThemedBackground(HDC hdc, LPCWSTR className, int part, int state,
                          RECT *prc) {
  HTHEME hTheme = NULL;
  if (IsThemeEnabled()) {
    hTheme = OpenThemeData(NULL, className);
  }

  if (hTheme) {
    DrawThemeBackground(hTheme, hdc, part, state, prc, NULL);
    CloseThemeData(hTheme);
    return TRUE;
  }

  return FALSE;
}

void DrawThemedButtonState(HDC hdc, RECT *prc, int state) {
  if (DrawThemedBackground(hdc, L"BUTTON", BP_PUSHBUTTON, state, prc)) {
    return;
  }

  UINT dfcsState = DFCS_BUTTONPUSH;
  if (state == PBS_PRESSED)
    dfcsState |= DFCS_PUSHED;
  FillRect(hdc, prc, GetSysColorBrush(COLOR_BTNFACE));
  DrawFrameControl(hdc, prc, DFC_BUTTON, dfcsState);
}

void DrawThemedButton(HDC hdc, RECT *prc, BOOL bPressed) {
  int state = bPressed ? PBS_PRESSED : PBS_NORMAL;
  DrawThemedButtonState(hdc, prc, state);
}

void ClearClientRect(HDC hdc, HWND hwnd, HBRUSH hBrush) {
  RECT rc;
  if (!hBrush) {
    hBrush = GetSysColorBrush(COLOR_BTNFACE);
  }
  GetClientRect(hwnd, &rc);
  FillRect(hdc, &rc, hBrush);
}

/*------------------------------------------------------------
    Drawing Primitives
------------------------------------------------------------*/

void DrawPixelAlpha(BYTE *bits, int width, int height, int x, int y,
                     COLORREF color, BYTE alpha, int mode) {
  if (!bits || x < 0 || y < 0 || x >= width || y >= height)
    return;
  if (alpha == 0)
    return;

  BYTE *px = bits + (y * width + x) * 4;
  int sr = GetRValue(color);
  int sg = GetGValue(color);
  int sb = GetBValue(color);
  PixelOps_BlendPixel(sr, sg, sb, alpha, px, mode);
}

void DrawLineSpineEach(BYTE *bits, int width, int height, int x1, int y1, int x2,
                       int y2, DrawLineSpineFn fn, void *userData) {
  int dx = abs(x2 - x1);
  int dy = abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;
  int x = x1;
  int y = y1;

  for (;;) {
    fn(bits, width, height, x, y, userData);
    if (x == x2 && y == y2)
      break;
    int e2 = 2 * err;
    if (e2 > -dy) {
      err -= dy;
      x += sx;
    }
    if (e2 < dx) {
      err += dx;
      y += sy;
    }
  }
}

typedef struct {
  int halfLo;
  int halfHi;
  COLORREF color;
  BYTE alpha;
  int mode;
} DrawLineAlphaThickCtx;

static void DrawLineAlphaThickVisit(BYTE *bits, int width, int height, int x,
                                    int y, void *userData) {
  DrawLineAlphaThickCtx *c = (DrawLineAlphaThickCtx *)userData;
  int r = c->halfLo;
  if (r <= 0) {
      DrawPixelAlpha(bits, width, height, x, y, c->color, c->alpha, c->mode);
  } else {
      DrawCircleAlpha(bits, width, height, x, y, r, c->color, c->alpha, c->mode);
  }
}

void DrawLineAlpha(BYTE *bits, int width, int height, int x1, int y1, int x2,
                   int y2, int thickness, COLORREF color, BYTE alpha,
                   int mode) {
  if (thickness <= 0)
    return;

  /* Symmetric odd/even thickness: e.g. 2 -> 2×2 stamp perpendicular to spine. */
  DrawLineAlphaThickCtx ctx;
  ctx.halfLo = thickness / 2;
  ctx.halfHi = (thickness - 1) / 2;
  ctx.color = color;
  ctx.alpha = alpha;
  ctx.mode = mode;
  DrawLineSpineEach(bits, width, height, x1, y1, x2, y2, DrawLineAlphaThickVisit,
                    &ctx);
}
 
void DrawCircleAlpha(BYTE *bits, int width, int height, int x, int y,
                     int radius, COLORREF color, BYTE alpha, int mode) {
  int r2 = radius * radius;
  for (int dy = -radius; dy <= radius; dy++) {
    for (int dx = -radius; dx <= radius; dx++) {
      if (dx * dx + dy * dy <= r2) {
        DrawPixelAlpha(bits, width, height, x + dx, y + dy, color, alpha, mode);
      }
    }
  }
}

void DrawLineStampCircles(BYTE *bits, int width, int height, float x1, float y1,
                          float x2, float y2, float radius, COLORREF color,
                          BYTE alpha, int mode) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  if (dx * dx + dy * dy < 0.01f) {
    int ir = (int)ceilf(radius);
    if (ir < 1)
      ir = 1;
    DrawCircleAlpha(bits, width, height, (int)x1, (int)y1, ir, color, alpha,
                    mode);
    return;
  }

  int thick = (int)ceilf(2.0f * radius);
  if (thick < 1)
    thick = 1;
  DrawLineAlpha(bits, width, height, (int)x1, (int)y1, (int)x2, (int)y2, thick,
                color, alpha, mode);
}

void DrawRectAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                    int h, COLORREF color, BYTE alpha, int mode) {
  if (!bits || w <= 0 || h <= 0)
    return;

  int sr = GetRValue(color);
  int sg = GetGValue(color);
  int sb = GetBValue(color);

  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < 0 || py >= height)
      continue;

    int startX = (x < 0) ? 0 : x;
    int endX = (x + w > width) ? width : x + w;

    BYTE *row = bits + py * width * 4;
    for (int px = startX; px < endX; px++) {
      PixelOps_BlendPixel(sr, sg, sb, alpha, row + px * 4, mode);
    }
  }
}
 
void DrawRectOutlineAlpha(BYTE *bits, int width, int height, int x, int y,
                          int w, int h, COLORREF color, BYTE alpha,
                          int thickness, int mode) {
  DrawRectAlpha(bits, width, height, x, y, w, thickness, color, alpha, mode);
  DrawRectAlpha(bits, width, height, x, y + h - thickness, w, thickness, color,
                alpha, mode);
  DrawRectAlpha(bits, width, height, x, y + thickness, thickness,
                h - 2 * thickness, color, alpha, mode);
  DrawRectAlpha(bits, width, height, x + w - thickness, y + thickness,
                thickness, h - 2 * thickness, color, alpha, mode);
}

void DrawEllipseAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                      int h, COLORREF color, BYTE alpha, BOOL bFill,
                      int thickness, int mode) {
  if (w <= 0 || h <= 0)
    return;
 
  float cx = x + w / 2.0f;
  float cy = y + h / 2.0f;
  float rx = w / 2.0f;
  float ry = h / 2.0f;
  float rx2 = rx * rx;
  float ry2 = ry * ry;
 
  int minX = x;
  int minY = y;
  int maxX = x + w;
  int maxY = y + h;
 
  if (minX < 0)
    minX = 0;
  if (minY < 0)
    minY = 0;
  if (maxX > width)
    maxX = width;
  if (maxY > height)
    maxY = height;
 
  for (int j = minY; j < maxY; j++) {
    for (int i = minX; i < maxX; i++) {
      float dx = (float)i - cx;
      float dy = (float)j - cy;
 
      float dist = (dx * dx) / rx2 + (dy * dy) / ry2;
 
      if (bFill) {
        if (dist <= 1.0f) {
          DrawPixelAlpha(bits, width, height, i, j, color, alpha, mode);
        }
      } else {
        float irx = rx - thickness;
        float iry = ry - thickness;
        if (irx < 0)
          irx = 0;
        if (iry < 0)
          iry = 0;
 
float irx2 = irx * irx;
         float iry2 = iry * iry;

         float distInner = (irx2 > 0 && iry2 > 0)
          ? ((dx * dx) / irx2 + (dy * dy) / iry2)
          : 999.0f;
 
        if (dist <= 1.0f && distInner >= 1.0f) {
          DrawPixelAlpha(bits, width, height, i, j, color, alpha, mode);
        }
      }
    }
  }
}
 
/* Rounded rectangle: IQ-style rounded-box SDF; outline = band -thick <= d <= 0. */
static float sd_rounded_rect(float px, float py, float half_w, float half_h,
                             float rad) {
  if (rad <= 0.f) {
    float ox = fabsf(px) - half_w;
    float oy = fabsf(py) - half_h;
    float ax = fmaxf(ox, 0.f);
    float ay = fmaxf(oy, 0.f);
    return sqrtf(ax * ax + ay * ay) + fminf(fmaxf(ox, oy), 0.f);
  }
  float bx = half_w - rad;
  float by = half_h - rad;
  float qx = fabsf(px) - bx;
  float qy = fabsf(py) - by;
  float ax = fmaxf(qx, 0.f);
  float ay = fmaxf(qy, 0.f);
  return sqrtf(ax * ax + ay * ay) - rad;
}

void DrawRoundedRectAlpha(BYTE *bits, int width, int height, int x, int y,
                          int w, int h, int radius, COLORREF color, BYTE alpha,
                          BOOL bFill, int thickness, int mode) {
  if (!bits || w <= 0 || h <= 0)
    return;

  if (radius * 2 > w)
    radius = w / 2;
  if (radius * 2 > h)
    radius = h / 2;
  if (radius < 0)
    radius = 0;

  float hw = w * 0.5f;
  float hh = h * 0.5f;
  float rad = (float)radius;
  int thick = thickness;
  if (!bFill && thick < 1)
    thick = 1;
  float band = (float)thick;

  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < 0 || py >= height)
      continue;

    for (int i = 0; i < w; i++) {
      int px = x + i;
      if (px < 0 || px >= width)
        continue;

      float pxf = (i + 0.5f) - hw;
      float pyf = (j + 0.5f) - hh;
      float d = sd_rounded_rect(pxf, pyf, hw, hh, rad);

      if (bFill) {
        if (d > 0.f)
          continue;
      } else {
        /* Outer boundary d==0; inward offset by thickness is d == -thick. */
        if (d > 0.f || d < -band)
          continue;
      }

      DrawPixelAlpha(bits, width, height, px, py, color, alpha, mode);
    }
  }
}

void EraseRectAlpha(BYTE *bits, int width, int height, int x, int y, int w,
                     int h) {
  if (!bits || w <= 0 || h <= 0)
    return;
  for (int j = 0; j < h; j++) {
    int py = y + j;
    if (py < 0 || py >= height)
      continue;
    int startX = (x < 0) ? 0 : x;
    int endX = (x + w > width) ? width : x + w;
    DWORD *row = (DWORD *)(bits + py * width * 4);
    for (int px = startX; px < endX; px++) {
      row[px] = 0;
    }
  }
}
