#ifndef FLOODFILL_H
#define FLOODFILL_H
#include <windows.h>
void FloodFillCanvas(int startX, int startY, COLORREF fillColor, BYTE fillAlpha);
void FloodFillCleanup(void);
#endif
