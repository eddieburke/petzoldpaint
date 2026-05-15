#ifndef DRAWING_PRIMITIVES_H
#define DRAWING_PRIMITIVES_H

#include "peztold_core.h"

#include <windows.h>


int DrawPrim_GetBrushSize(int brushWidthIndex);
int DrawPrim_GetBrushRow(int brushWidthIndex);
int DrawPrim_GetSprayRadius(int sprayRadiusIndex);



void DrawPrim_DrawPencilPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, BYTE alpha);
void DrawPrim_DrawPencilLine(BYTE *bits, int width, int height, int x1, int y1,
                             int x2, int y2, COLORREF color, BYTE alpha);


void DrawPrim_DrawEraserPoint(BYTE *bits, int width, int height, int x, int y,
                              COLORREF color, int brushWidthIndex);


void DrawPrim_DrawBrushPoint(BYTE *bits, int width, int height, int x, int y,
                             COLORREF color, BYTE alpha, int brushWidthIndex);
void DrawPrim_DrawBrushLine(BYTE *bits, int width, int height, int x1, int y1,
                            int x2, int y2, COLORREF color, BYTE alpha,
                            int brushWidthIndex);


void DrawPrim_DrawSprayPoint(BYTE *bits, int width, int height, int x, int y,
                             COLORREF color, BYTE alpha, int sprayRadiusIndex);

#endif
