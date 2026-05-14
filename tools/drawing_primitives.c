/*------------------------------------------------------------------------------
 * DRAWING_PRIMITIVES.C
 *
 * Shared Drawing Primitives
 *
 * Implements low-level drawing logic for freehand tools (Pencil, Brush,
 * Eraser, Airbrush). Handles size calculation and pixel/shape rendering.
 *----------------------------------------------------------------------------*/

#include "drawing_primitives.h"
#include "../draw.h"
#include "../layers.h"
#include <stdlib.h>

/*------------------------------------------------------------------------------
 * Brush Size Helpers
 *----------------------------------------------------------------------------*/

int DrawPrim_GetBrushSize(int brushWidthIndex) {
   if (brushWidthIndex < 1) brushWidthIndex = 1;
   int col = (brushWidthIndex - 1) % 3;
   if (col == 0)
     return 1;
   if (col == 1)
     return 3;
   return 5;
}

int DrawPrim_GetBrushRow(int brushWidthIndex) {
   if (brushWidthIndex < 1) brushWidthIndex = 1;
   return (brushWidthIndex - 1) / 3;
}


/*------------------------------------------------------------------------------
 * Pencil Implementation
 *----------------------------------------------------------------------------*/

void DrawPrim_DrawPencilPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color) {
  DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
}

void DrawPrim_DrawPencilLine(BYTE *bits, int width, int height, int x1, int y1,
                             int x2, int y2, COLORREF color) {
  DrawLineAlpha(bits, width, height, x1, y1, x2, y2, 1, color, 255, LAYER_BLEND_NORMAL);
}

/*------------------------------------------------------------------------------
 * Eraser Implementation
 *----------------------------------------------------------------------------*/

void DrawPrim_DrawEraserPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, int brushWidthIndex) {
  int sizes[] = {4, 6, 8, 10};
  int size;
   if (brushWidthIndex < 1)
     size = sizes[0];
   else
     size = sizes[(brushWidthIndex - 1) % 4];
  (void)color; // Eraser uses background color or clears alpha, handled by
               // EraseRectAlpha
  EraseRectAlpha(bits, width, height, x, y - size, size, size);
}

/*------------------------------------------------------------------------------
 * Brush Point & Line Implementation
 *----------------------------------------------------------------------------*/

void DrawPrim_DrawBrushPoint(BYTE *bits, int width, int height, int x, int y,
                             COLORREF color, int brushWidthIndex) {
  int size = DrawPrim_GetBrushSize(brushWidthIndex);
  int row = DrawPrim_GetBrushRow(brushWidthIndex);

  switch (row) {
  case 0: // Circle/Dot
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    } else {
      DrawCircleAlpha(bits, width, height, x, y, (size - 1) / 2, color, 255, LAYER_BLEND_NORMAL);
    }
    break;

  case 1: // Square
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    } else {
      DrawRectAlpha(bits, width, height, x - size / 2, y - size / 2, size, size,
                    color, 255, LAYER_BLEND_NORMAL);
    }
    break;

  case 2: // Backward Diagonal Line (/)
  {
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    } else {
      int w = size / 2;
      DrawLineAlpha(bits, width, height, x + w, y - w, x - w, y + w, 1, color,
                    255, LAYER_BLEND_NORMAL);
    }
    break;
  }

  case 3: // Forward Diagonal Line (\)
  {
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, 255, LAYER_BLEND_NORMAL);
    } else {
      int w = size / 2;
      DrawLineAlpha(bits, width, height, x - w, y - w, x + w, y + w, 1, color,
                    255, LAYER_BLEND_NORMAL);
    }
    break;
  }
  }
}

void DrawPrim_DrawBrushLine(BYTE *bits, int width, int height, int x1, int y1,
                            int x2, int y2, COLORREF color,
                            int brushWidthIndex) {
  int dx = x2 - x1;
  int dy = y2 - y1;
  int adx = (dx >= 0) ? dx : -dx;
  int ady = (dy >= 0) ? dy : -dy;
  int steps = adx + ady; // Manhattan distance for 4-connectivity (no gaps)

  if (steps == 0) {
    DrawPrim_DrawBrushPoint(bits, width, height, x1, y1, color,
                            brushWidthIndex);
    return;
  }

  for (int i = 1; i <= steps; i++) {
    int ix = x1 + (dx * i + (steps / 2 * (dx < 0 ? -1 : 1))) / steps;
    int iy = y1 + (dy * i + (steps / 2 * (dy < 0 ? -1 : 1))) / steps;
    DrawPrim_DrawBrushPoint(bits, width, height, ix, iy, color,
                            brushWidthIndex);
  }
}

/*------------------------------------------------------------------------------
 * Spray / Airbrush Implementation
 *----------------------------------------------------------------------------*/

void DrawPrim_DrawSprayPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, int sprayRadiusIndex) {
   int radii[] = {5, 8, 12};
   int densities[] = {10, 20, 30};
   int idx = sprayRadiusIndex - 1;
   if (idx < 0) idx = 0;
   int radius = radii[idx % 3];
   int density = densities[idx % 3];

  for (int i = 0; i < density; i++) {
    // Generate random offset within circle
    int dx = (rand() % (radius * 2)) - radius;
    int dy = (rand() % (radius * 2)) - radius;

    if (dx * dx + dy * dy <= radius * radius) {
      DrawPixelAlpha(bits, width, height, x + dx, y + dy, color, 255, LAYER_BLEND_NORMAL);
    }
  }
}
