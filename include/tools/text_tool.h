#ifndef TEXT_TOOL_H
#define TEXT_TOOL_H
#include "peztold_core.h"
#include "text_font.h"
#include "text_toolbar.h"
#include "tools.h"
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
void TextTool_OnPointer(const ToolPointerEvent *ev);
void TextTool_OnViewportChanged(HWND hWnd);
BOOL TextTool_HandleOverlayClick(HWND hWnd, int screenX, int screenY, int nButton);
void TextTool_DrawOverlay(const OverlayContext *ctx);
BOOL TextTool_Cancel(void);
BOOL IsTextEditing(void);
BOOL TextTool_TryEditUndo(void);
void TextTool_Deactivate(void);
TextToolSnapshot *TextTool_CreateSnapshot(void);
void TextTool_DestroySnapshot(TextToolSnapshot *snapshot);
void TextTool_ApplySnapshot(const TextToolSnapshot *snapshot);
#endif
