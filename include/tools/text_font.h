#ifndef TEXT_FONT_H
#define TEXT_FONT_H

#include <windows.h>


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

/* Create/update font based on current state */
void TextFont_Apply(void);

/* Set font face name */
void TextFont_SetFaceName(const char* faceName);

/* Set font size */
void TextFont_SetSize(int size);

/* Get current LOGFONT */
void TextFont_GetLogFont(LOGFONT* pLf);

/* Initialize font system */
void TextFont_Init(void);

#endif /* TEXT_FONT_H */
