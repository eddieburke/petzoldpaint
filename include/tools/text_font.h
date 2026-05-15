#ifndef TEXT_FONT_H
#define TEXT_FONT_H

#include <windows.h>


typedef struct {
    HFONT hFont;
    LOGFONT lf;
    BOOL bBold;
    BOOL bItalic;
    BOOL bUnderline;
    BOOL bStrikeout;
    int nSize;
} TextFontState;

void TextFont_Apply(void);

void TextFont_SetFaceName(const char* faceName);

void TextFont_SetSize(int size);

void TextFont_GetLogFont(LOGFONT* pLf);

void TextFont_Init(void);

#endif
