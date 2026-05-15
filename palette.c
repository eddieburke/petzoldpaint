/*------------------------------------------------------------
   PALETTE.C -- Color Palette Management
   
   This module manages the custom color palette and provides
   the color selection dialog.
  ------------------------------------------------------------*/

#include "palette.h"
#include <commdlg.h>

/*------------------------------------------------------------
   Custom Color Storage
  ------------------------------------------------------------*/

static COLORREF customColors[16];

/*------------------------------------------------------------
   InitializePalette
   
   Initializes the custom color palette to white.
  ------------------------------------------------------------*/

void InitializePalette(void)
{
    for (int i = 0; i < 16; i++) {
        customColors[i] = RGB(255, 255, 255);
    }
}

/*------------------------------------------------------------
   ChooseColorDialog
   
   Displays the Windows color selection dialog and returns
   the selected color. Returns TRUE if a color was selected.
  ------------------------------------------------------------*/

BOOL ChooseColorDialog(HWND hWnd, COLORREF* color)
{
    CHOOSECOLOR cc;
    ZeroMemory(&cc, sizeof(cc));
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hWnd;
    cc.lpCustColors = customColors;
    cc.rgbResult = *color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColor(&cc)) {
        *color = cc.rgbResult;
        return TRUE;
    }
    return FALSE;
}

/*------------------------------------------------------------
   SetCustomColors
   
   Sets the custom color palette from an array of colors.
  ------------------------------------------------------------*/

void SetCustomColors(const COLORREF* colors)
{
    for (int i = 0; i < 16; i++) {
        customColors[i] = colors[i];
    }
}

static COLORREF g_primaryColor = RGB(0, 0, 0);
static COLORREF g_secondaryColor = RGB(255, 255, 255);
static BYTE g_primaryOpacity = 255;
static BYTE g_secondaryOpacity = 255;

COLORREF Palette_GetPrimaryColor(void) { return g_primaryColor; }
void Palette_SetPrimaryColor(COLORREF c) { g_primaryColor = c; }
COLORREF* Palette_GetPrimaryColorPtr(void) { return &g_primaryColor; }
BYTE Palette_GetPrimaryOpacity(void) { return g_primaryOpacity; }
void Palette_SetPrimaryOpacity(BYTE opacity) { g_primaryOpacity = opacity; }

COLORREF Palette_GetSecondaryColor(void) { return g_secondaryColor; }
void Palette_SetSecondaryColor(COLORREF c) { g_secondaryColor = c; }
COLORREF* Palette_GetSecondaryColorPtr(void) { return &g_secondaryColor; }
BYTE Palette_GetSecondaryOpacity(void) { return g_secondaryOpacity; }
void Palette_SetSecondaryOpacity(BYTE opacity) { g_secondaryOpacity = opacity; }
