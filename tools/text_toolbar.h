#ifndef TEXT_TOOLBAR_H
#define TEXT_TOOLBAR_H

#include <windows.h>

/*------------------------------------------------------------
   Text Toolbar Management
   
   Handles the floating toolbar window for text tool formatting
   options (font, size, bold, italic, underline, strikeout, opaque/transparent).
------------------------------------------------------------*/

/* Toolbar control IDs */
#define IDC_TEXT_FONT 1001
#define IDC_TEXT_SIZE 1002
#define IDC_TEXT_BOLD 1003
#define IDC_TEXT_ITALIC 1004
#define IDC_TEXT_UNDERLINE 1005
#define IDC_TEXT_STRIKEOUT 1008
#define IDC_TEXT_OPAQUE 1006
#define IDC_TEXT_TRANSPARENT 1007

/* Destroy toolbar window */
void TextToolbar_Destroy(void);

/* Show/hide toolbar */
void TextToolbar_Show(BOOL bShow);

/* Get toolbar window handle (for compatibility) */
HWND TextToolbar_GetHwnd(void);

/* Check if toolbar is visible (for compatibility) */
BOOL TextToolbar_IsVisible(void);

/* Compatibility aliases */
#define GetTextToolbar TextToolbar_GetHwnd
#define IsTextToolbarVisible TextToolbar_IsVisible

/* Update button states to reflect current font settings */
void TextToolbar_UpdateButtonStates(void);

#endif /* TEXT_TOOLBAR_H */
