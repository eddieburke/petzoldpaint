#ifndef HIGHLIGHTER_TOOL_H
#define HIGHLIGHTER_TOOL_H
#include "peztold_core.h"
#include "tools.h"
void HighlighterTool_OnPointer(const ToolPointerEvent *ev);
BOOL IsHighlighterDrawing(void);
void HighlighterTool_Deactivate(void);
BOOL HighlighterTool_Cancel(void);
void HighlighterTool_RegisterPresets(void);
void HighlighterOptions_Draw(HDC hdc, RECT *prc);
BOOL HighlighterOptions_LButtonDown(HWND hwnd, int x, int y);
BOOL HighlighterOptions_MouseMove(HWND hwnd, int y);
#endif
