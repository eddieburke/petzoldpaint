#include "palette.h"
#include "gdi_utils.h"
#include <math.h>
#include <stdio.h>
static COLORREF g_primaryColor = RGB(0, 0, 0);
static COLORREF g_secondaryColor = RGB(255, 255, 255);
static BYTE g_primaryOpacity = 255;
static BYTE g_secondaryOpacity = 255;
static COLORREF s_chooseCustColors[16];
void SetCustomColors(const COLORREF *colors) {
	if (!colors)
		return;
	for (int i = 0; i < 16; i++)
		s_chooseCustColors[i] = colors[i];
}
COLORREF *Palette_GetPrimaryColorPtr(void) {
	return &g_primaryColor;
}
COLORREF *Palette_GetSecondaryColorPtr(void) {
	return &g_secondaryColor;
}
COLORREF
Palette_GetPrimaryColor(void) {
	return g_primaryColor;
}
void Palette_SetPrimaryColor(COLORREF c) {
	g_primaryColor = c;
}
BYTE Palette_GetPrimaryOpacity(void) {
	return g_primaryOpacity;
}
void Palette_SetPrimaryOpacity(BYTE a) {
	g_primaryOpacity = a;
}
COLORREF
Palette_GetSecondaryColor(void) {
	return g_secondaryColor;
}
void Palette_SetSecondaryColor(COLORREF c) {
	g_secondaryColor = c;
}
BYTE Palette_GetSecondaryOpacity(void) {
	return g_secondaryOpacity;
}
void Palette_SetSecondaryOpacity(BYTE a) {
	g_secondaryOpacity = a;
}
BOOL ChooseColorDialog(HWND hWnd, COLORREF *color) {
	CHOOSECOLOR
	cc;
	ZeroMemory(&cc, sizeof(cc));
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = hWnd;
	cc.lpCustColors = (LPDWORD)s_chooseCustColors;
	cc.rgbResult = *color;
	cc.Flags = CC_FULLOPEN | CC_RGBINIT;
	if (ChooseColor(&cc) == TRUE) {
		*color = cc.rgbResult;
		return TRUE;
	}
	return FALSE;
}
