#ifndef PALETTE_H
#define PALETTE_H
#include <windows.h>
#include "peztold_core.h"

typedef struct {
    COLORREF colors[16];
} CustomColors;

BOOL ChooseColorDialog(HWND hWnd, COLORREF* color);
void SetCustomColors(const COLORREF* colors);

typedef struct {
    BYTE r, g, b, a;
} RGBA;

COLORREF Palette_GetPrimaryColor(void);
void Palette_SetPrimaryColor(COLORREF c);
COLORREF* Palette_GetPrimaryColorPtr(void);
BYTE Palette_GetPrimaryOpacity(void);
void Palette_SetPrimaryOpacity(BYTE opacity);

COLORREF Palette_GetSecondaryColor(void);
void Palette_SetSecondaryColor(COLORREF c);
COLORREF* Palette_GetSecondaryColorPtr(void);
BYTE Palette_GetSecondaryOpacity(void);
void Palette_SetSecondaryOpacity(BYTE opacity);

BOOL Palette_ChooseColorWithAlpha(HWND hWnd, RGBA* outRgba);

#endif
