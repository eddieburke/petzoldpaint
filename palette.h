#ifndef PALETTE_H
#define PALETTE_H
#include <windows.h>
#include "peztold_core.h"

typedef struct {
    COLORREF colors[16];
} CustomColors;
void InitializePalette();
BOOL ChooseColorDialog(HWND hWnd, COLORREF* color);
void SetCustomColors(const COLORREF* colors);

COLORREF Palette_GetPrimaryColor(void);
void Palette_SetPrimaryColor(COLORREF c);
COLORREF* Palette_GetPrimaryColorPtr(void);

COLORREF Palette_GetSecondaryColor(void);
void Palette_SetSecondaryColor(COLORREF c);
COLORREF* Palette_GetSecondaryColorPtr(void);

#endif
