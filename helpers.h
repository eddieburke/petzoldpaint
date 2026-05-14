/*------------------------------------------------------------
    Helpers.h - App helpers (tool/history, invalidation, drawing state)
    For drawing primitives use draw.h or gdi_utils.h.
    For geometry use geom.h.
------------------------------------------------------------*/

#ifndef HELPERS_H
#define HELPERS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <windows.h>

/*------------------------------------------------------------
    Common Utilities
------------------------------------------------------------*/

COLORREF GetColorForButton(int nButton);

/*------------------------------------------------------------
    Invalidation
------------------------------------------------------------*/

void InvalidateWindow(HWND hwnd);
void InvalidateCanvas(void);
void InvalidateCanvasRect(RECT *pRect);

/* Ensure canvas updates properly after pixel modifications */
void UpdateCanvasAfterModification(void);



/*------------------------------------------------------------
    History Helper Functions
------------------------------------------------------------*/

void HistoryPushFormatted(const char *format, ...);
void HistoryPushToolAction(const char *toolName, const char *action);
void HistoryPushToolActionById(int toolId, const char *action);
const char *GetToolName(int toolId);

#endif /* HELPERS_H */
