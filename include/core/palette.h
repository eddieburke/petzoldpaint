#ifndef PALETTE_H
#define PALETTE_H
#include <windows.h>
#include "peztold_core.h"

BOOL ChooseColorDialog(HWND hWnd, COLORREF* color);

/* Copies the first 16 colors into ChooseColorDialog's custom palette slots. */
void SetCustomColors(const COLORREF *colors);

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

#endif
