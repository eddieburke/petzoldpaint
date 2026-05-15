#include "drawing_primitives.h"
#include "draw.h"
#include "layers.h"
#include <stdlib.h>


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

int DrawPrim_GetSprayRadius(int sprayRadiusIndex) {
  int radii[] = {5, 8, 12};
  int idx = sprayRadiusIndex - 1;
  if (idx < 0) idx = 0;
  if (idx > 2) idx = 2;
  return radii[idx];
}



void DrawPrim_DrawPencilPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, BYTE alpha) {
  DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
}

void DrawPrim_DrawPencilLine(BYTE *bits, int width, int height, int x1, int y1,
                             int x2, int y2, COLORREF color, BYTE alpha) {
  DrawLineAlpha(bits, width, height, x1, y1, x2, y2, 1, color, alpha, LAYER_BLEND_NORMAL);
}


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
  int half = size / 2;
  EraseRectAlpha(bits, width, height, x - half, y - half, size, size);
}


void DrawPrim_DrawBrushPoint(BYTE *bits, int width, int height, int x, int y,
                             COLORREF color, BYTE alpha, int brushWidthIndex) {
  int size = DrawPrim_GetBrushSize(brushWidthIndex);
  int row = DrawPrim_GetBrushRow(brushWidthIndex);

  switch (row) {
  case 0: // Circle/Dot
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    } else {
      DrawCircleAlpha(bits, width, height, x, y, (size - 1) / 2, color, alpha, LAYER_BLEND_NORMAL);
    }
    break;

  case 1: // Square
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    } else {
      DrawRectAlpha(bits, width, height, x - size / 2, y - size / 2, size, size,
                    color, alpha, LAYER_BLEND_NORMAL);
    }
    break;

  case 2: // Backward Diagonal Line (/)
  {
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    } else {
      int w = size / 2;
      DrawLineAlpha(bits, width, height, x + w, y - w, x - w, y + w, 1, color,
                    alpha, LAYER_BLEND_NORMAL);
    }
    break;
  }

  case 3: // Forward Diagonal Line (\)
  {
    if (size == 1) {
      DrawPixelAlpha(bits, width, height, x, y, color, alpha, LAYER_BLEND_NORMAL);
    } else {
      int w = size / 2;
      DrawLineAlpha(bits, width, height, x - w, y - w, x + w, y + w, 1, color,
                    alpha, LAYER_BLEND_NORMAL);
    }
    break;
  }
  }
}

typedef struct {
  COLORREF color;
  BYTE alpha;
  int brushWidthIndex;
} DrawPrim_BrushLineCtx;

static void DrawPrim_BrushLineVisit(BYTE *bits, int width, int height, int x,
                                    int y, void *userData) {
  DrawPrim_BrushLineCtx *c = (DrawPrim_BrushLineCtx *)userData;
  DrawPrim_DrawBrushPoint(bits, width, height, x, y, c->color, c->alpha,
                          c->brushWidthIndex);
}

void DrawPrim_DrawBrushLine(BYTE *bits, int width, int height, int x1, int y1,
                            int x2, int y2, COLORREF color, BYTE alpha,
                            int brushWidthIndex) {
  if (x1 == x2 && y1 == y2) {
    DrawPrim_DrawBrushPoint(bits, width, height, x1, y1, color, alpha,
                            brushWidthIndex);
    return;
  }

  DrawPrim_BrushLineCtx ctx = {color, alpha, brushWidthIndex};
  DrawLineSpineEach(bits, width, height, x1, y1, x2, y2, DrawPrim_BrushLineVisit,
                    &ctx);
}


void DrawPrim_DrawSprayPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, BYTE alpha, int sprayRadiusIndex) {
   int densities[] = {10, 20, 30};
   int idx = sprayRadiusIndex - 1;
   if (idx < 0) idx = 0;
   if (idx > 2) idx = 2;
   int radius = DrawPrim_GetSprayRadius(sprayRadiusIndex);
   int density = densities[idx % 3];

  for (int i = 0; i < density; i++) {
    // Generate random offset within circle
    int dx = (rand() % (radius * 2)) - radius;
    int dy = (rand() % (radius * 2)) - radius;

    if (dx * dx + dy * dy <= radius * radius) {
      DrawPixelAlpha(bits, width, height, x + dx, y + dy, color, alpha, LAYER_BLEND_NORMAL);
    }
  }
}
