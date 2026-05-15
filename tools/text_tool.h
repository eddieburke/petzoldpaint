#ifndef TEXT_TOOL_H
#define TEXT_TOOL_H

#include "../peztold_core.h"

#include "text_font.h"
#include "text_toolbar.h"
#include <windows.h>

/*------------------------------------------------------------------------------
 * Global Font State
 *----------------------------------------------------------------------------*/

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

/*------------------------------------------------------------------------------
 * Event Handlers
 *----------------------------------------------------------------------------*/

void TextToolOnMouseDown(HWND hWnd, int x, int y, int nButton);
void TextToolOnMouseMove(HWND hWnd, int x, int y, int nButton);
void TextToolOnMouseUp(HWND hWnd, int x, int y, int nButton);
void TextToolOnViewportChanged(HWND hWnd);

/*------------------------------------------------------------------------------
 * Rendering & Overlay
 *----------------------------------------------------------------------------*/

void TextToolDrawOverlay(HDC hdc, double dScale, int nDestX, int nDestY);

/*------------------------------------------------------------------------------
 * State Management / Actions
 *----------------------------------------------------------------------------*/

void CommitText(HWND hWndParent);
BOOL CancelText(void);
BOOL IsTextEditing(void);
void ApplyTextFont(void);
void TextTool_Deactivate(void);
TextToolSnapshot *TextTool_CreateSnapshot(void);
void TextTool_DestroySnapshot(TextToolSnapshot *snapshot);
void TextTool_ApplySnapshot(const TextToolSnapshot *snapshot);

#endif
