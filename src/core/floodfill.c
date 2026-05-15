#include "floodfill.h"
#include "canvas.h"
#include "helpers.h"
#include "layers.h"
#include "tools/selection_tool.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int x1, x2, y, dy;
} Span;

static Span *s_stack = NULL;
static size_t s_stackCount = 0;
static size_t s_stackCap = 0;

static BOOL StackPush(int x1, int x2, int y, int dy) {
  if (s_stackCount >= s_stackCap)
    return FALSE;
  s_stack[s_stackCount++] = (Span){x1, x2, y, dy};
  return TRUE;
}

BOOL FloodFillCanvas(int startX, int startY, COLORREF fillColor,
                       BYTE fillAlpha) {
  BYTE *bits = LayersGetActiveColorBits();
  if (!bits)
    return FALSE;

  int w = Canvas_GetWidth();
  int h = Canvas_GetHeight();

  if (startX < 0 || startX >= w || startY < 0 || startY >= h)
    return FALSE;

  BYTE *startPx = bits + (startY * w + startX) * 4;
  DWORD startPixel = *(DWORD *)startPx;
  BYTE fillR = GetRValue(fillColor);
  BYTE fillG = GetGValue(fillColor);
  BYTE fillB = GetBValue(fillColor);
  DWORD fillPixel =
      (DWORD)(fillB | (fillG << 8) | (fillR << 16) | (fillAlpha << 24));

  if (startPixel == fillPixel)
    return FALSE;

  BOOL hasSelection = IsSelectionActive();
  if (hasSelection && !IsPointInSelection(startX, startY))
    return FALSE;

  size_t needed = (size_t)w * (size_t)h + 2;
  if (needed > SIZE_MAX / sizeof(Span))
    return FALSE;
  free(s_stack);
  s_stack = (Span *)malloc(needed * sizeof(Span));
  if (!s_stack) {
    s_stackCap = 0;
    return FALSE;
  }
  s_stackCount = 0;
  s_stackCap = needed;

  if (!StackPush(startX, startX, startY, 1))
    goto cleanup_fail;
  if (!StackPush(startX, startX, startY - 1, -1))
    goto cleanup_fail;

  while (s_stackCount > 0) {
    Span s = s_stack[--s_stackCount];
    int x = s.x1;
    int y = s.y;
    if (y < 0 || y >= h) continue;

    int l = x;
    while (l >= 0 && *(DWORD*)(bits + (y * w + l) * 4) == startPixel && 
           (!hasSelection || IsPointInSelection(l, y))) {
        *(DWORD*)(bits + (y * w + l) * 4) = fillPixel;
        l--;
    }
    l++;

    if (l < x) {
        if (!StackPush(l, x - 1, y - s.dy, -s.dy))
          goto cleanup_fail;
    }

    while (x < w) {
        while (x < w && *(DWORD*)(bits + (y * w + x) * 4) == startPixel && 
               (!hasSelection || IsPointInSelection(x, y))) {
            *(DWORD*)(bits + (y * w + x) * 4) = fillPixel;
            x++;
        }
        if (!StackPush(l, x - 1, y + s.dy, s.dy))
          goto cleanup_fail;
        if (x - 1 > s.x2) {
            if (!StackPush(s.x2 + 1, x - 1, y - s.dy, -s.dy))
              goto cleanup_fail;
        }
        x++;
        while (x <= s.x2 && (*(DWORD*)(bits + (y * w + x) * 4) != startPixel || 
               (hasSelection && !IsPointInSelection(x, y)))) {
            x++;
        }
        l = x;
    }
  }

  free(s_stack);
  s_stack = NULL;
  s_stackCount = 0;
  s_stackCap = 0;

  LayersMarkDirty();
  return TRUE;

cleanup_fail:
  free(s_stack);
  s_stack = NULL;
  s_stackCount = 0;
  s_stackCap = 0;
  return FALSE;
}

void FloodFillCleanup(void) {
  free(s_stack);
  s_stack = NULL;
  s_stackCount = 0;
  s_stackCap = 0;
}
