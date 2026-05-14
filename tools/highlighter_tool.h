#ifndef HIGHLIGHTER_TOOL_H
#define HIGHLIGHTER_TOOL_H

#include "../peztold_core.h"

/*------------------------------------------------------------------------------
 * Highlighter Tool API
 *----------------------------------------------------------------------------*/

void HighlighterToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void HighlighterToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void HighlighterToolOnMouseUp(HWND hWnd, int x, int y, int nButton);

BOOL IsHighlighterDrawing(void);
void HighlighterTool_Deactivate(void);
BOOL CancelHighlighterDrawing(void);

void HighlighterTool_RegisterPresets(void);

/*------------------------------------------------------------------------------
 * Highlighter Options API
 *----------------------------------------------------------------------------*/

void HighlighterOptions_Draw(HDC hdc, RECT *prc);
BOOL HighlighterOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL HighlighterOptions_MouseMove(HWND hwnd, int y);

#endif
