#ifndef HIGHLIGHTER_TOOL_H
#define HIGHLIGHTER_TOOL_H

#include "peztold_core.h"


void HighlighterTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void HighlighterTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void HighlighterTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);

BOOL IsHighlighterDrawing(void);
void HighlighterTool_Deactivate(void);
BOOL HighlighterTool_Cancel(void);

void HighlighterTool_RegisterPresets(void);


void HighlighterOptions_Draw(HDC hdc, RECT *prc);
BOOL HighlighterOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL HighlighterOptions_MouseMove(HWND hwnd, int y);

#endif
