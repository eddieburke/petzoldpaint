#ifndef TEXT_TOOL_H
#define TEXT_TOOL_H

#include "peztold_core.h"

#include "text_font.h"
#include "text_toolbar.h"
#include <windows.h>


extern HFONT hTextFont;
extern LOGFONT lfTextFont;
extern BOOL bTextBold;
extern BOOL bTextItalic;
extern BOOL bTextUnderline;
extern BOOL bTextStrikeout;
extern int nTextFontSize;

typedef struct TextToolSnapshot {
  int mode;
  RECT rcBox;
  BOOL bOpaque;
  TextFontState font;
  char *text;
} TextToolSnapshot;


void TextTool_OnMouseDown(HWND hWnd, int x, int y, int nButton);
void TextTool_OnMouseMove(HWND hWnd, int x, int y, int nButton);
void TextTool_OnMouseUp(HWND hWnd, int x, int y, int nButton);
void TextTool_OnViewportChanged(HWND hWnd);


void TextTool_DrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);


BOOL TextTool_Cancel(void);
BOOL IsTextEditing(void);
BOOL TextTool_TryEditUndo(void);
void TextTool_Deactivate(void);
TextToolSnapshot *TextTool_CreateSnapshot(void);
void TextTool_DestroySnapshot(TextToolSnapshot *snapshot);
void TextTool_ApplySnapshot(const TextToolSnapshot *snapshot);

#endif
