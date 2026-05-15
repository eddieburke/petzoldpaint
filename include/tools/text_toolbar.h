#ifndef TEXT_TOOLBAR_H
#define TEXT_TOOLBAR_H

#include <windows.h>


/* Toolbar control IDs */
#define IDC_TEXT_FONT 1001
#define IDC_TEXT_SIZE 1002
#define IDC_TEXT_BOLD 1003
#define IDC_TEXT_ITALIC 1004
#define IDC_TEXT_UNDERLINE 1005
#define IDC_TEXT_STRIKEOUT 1008
#define IDC_TEXT_OPAQUE 1006
#define IDC_TEXT_TRANSPARENT 1007

/* Show/hide toolbar */
void TextToolbar_Show(BOOL bShow);

/* Check if toolbar is visible (for compatibility) */
BOOL TextToolbar_IsVisible(void);

#endif /* TEXT_TOOLBAR_H */
