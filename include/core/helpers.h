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
BYTE GetOpacityForButton(int nButton);
BYTE ComposeOpacity(BYTE baseAlpha, BYTE colorOpacity);

/*------------------------------------------------------------
    Invalidation
------------------------------------------------------------*/
/* Ensure canvas updates properly after pixel modifications */
void UpdateCanvasAfterModification(void);



#endif /* HELPERS_H */
