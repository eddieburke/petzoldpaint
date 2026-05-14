/*------------------------------------------------------------
   FLOODFILL.C -- Flood Fill Algorithm Implementation

   This module implements the flood fill algorithm using a
   stack-based DFS approach for better performance and
   simpler memory management.
  ------------------------------------------------------------*/

#include "floodfill.h"
#include "canvas.h"
#include "helpers.h"
#include "layers.h"
#include "tools/selection_tool.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  int x, y;
} Point;

static Point *s_stack = NULL;
static int s_stackCount = 0;
static int s_stackCap = 0;

static inline void StackPush(int x, int y) {
  if (s_stackCount >= s_stackCap)
    return; // Prevent overflow
  s_stack[s_stackCount++] = (Point){x, y};
}

/*------------------------------------------------------------
   FloodFillCanvas

   Fills a connected region of pixels with the specified color.
  ------------------------------------------------------------*/

void FloodFillCanvas(int startX, int startY, COLORREF fillColor,
                     BYTE fillAlpha) {
  BYTE *bits = LayersGetActiveColorBits();
  if (!bits)
    return;

  int w = Canvas_GetWidth();
  int h = Canvas_GetHeight();

  if (startX < 0 || startX >= w || startY < 0 || startY >= h)
    return;

  BYTE *startPx = bits + (startY * w + startX) * 4;
  DWORD startPixel = *(DWORD *)startPx;
  BYTE fillR = GetRValue(fillColor);
  BYTE fillG = GetGValue(fillColor);
  BYTE fillB = GetBValue(fillColor);
  DWORD fillPixel =
      (DWORD)(fillB | (fillG << 8) | (fillR << 16) | (fillAlpha << 24));

  // Early exit if clicking on already-filled color
  if (startPixel == fillPixel) {
    return;
  }

  // Check if starting point is inside selection
  if (!IsPointInSelection(startX, startY)) {
    return;
  }

    // Allocate stack
    s_stackCap = (size_t)w * h;
    s_stack = (Point *)malloc(s_stackCap * sizeof(Point));
  if (!s_stack)
    return;
  s_stackCount = 0;

  // Set start pixel and push
  *(DWORD *)startPx = fillPixel;
  StackPush(startX, startY);

  // Process stack
  while (s_stackCount > 0) {
    Point p = s_stack[--s_stackCount]; // Pop
    int x = p.x;
    int y = p.y;

    struct {
      int nx, ny;
    } neighbors[] = {{x - 1, y}, {x + 1, y}, {x, y - 1}, {x, y + 1}};

    for (int i = 0; i < 4; i++) {
      int nx = neighbors[i].nx;
      int ny = neighbors[i].ny;

      if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
        DWORD *pPixel = (DWORD *)(bits + (ny * w + nx) * 4);
        if (*pPixel == startPixel && IsPointInSelection(nx, ny)) {
          *pPixel = fillPixel;
          StackPush(nx, ny);
        }
      }
    }
  }

  free(s_stack);
  s_stack = NULL;
  s_stackCount = 0;
  s_stackCap = 0;

  LayersMarkDirty();
}

void FloodFillCleanup(void) {
  // Now a no-op as memory is managed per-call
}
