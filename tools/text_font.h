#ifndef TEXT_FONT_H
#define TEXT_FONT_H

#include <windows.h>

/*------------------------------------------------------------
   Text Font Management
   
   Handles font creation and state management for text tool.
------------------------------------------------------------*/

/* Font state structure */
typedef struct {
    HFONT hFont;
    LOGFONT lf;
    BOOL bBold;
    BOOL bItalic;
    BOOL bUnderline;
    BOOL bStrikeout;
    int nSize;
} TextFontState;

/* Get current font state (for toolbar access) */
TextFontState* TextFont_GetState(void);

/* Create/update font based on current state */
void TextFont_Apply(void);

/* Set font face name */
void TextFont_SetFaceName(const char* faceName);

/* Set font size */
void TextFont_SetSize(int size);

/* Toggle bold */
void TextFont_ToggleBold(void);

/* Toggle italic */
void TextFont_ToggleItalic(void);

/* Toggle underline */
void TextFont_ToggleUnderline(void);

/* Toggle strikeout */
void TextFont_ToggleStrikeout(void);

/* Get current font handle */
HFONT TextFont_GetHandle(void);

/* Get current LOGFONT */
void TextFont_GetLogFont(LOGFONT* pLf);

/* Initialize font system */
void TextFont_Init(void);

/* Cleanup font system */
void TextFont_Cleanup(void);

#endif /* TEXT_FONT_H */
