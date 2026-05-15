/*------------------------------------------------------------
    Helpers.c - App helpers (tool state, history, invalidation)
------------------------------------------------------------*/

#include "peztold_core.h"
#include "canvas.h"
#include "history.h"
#include "layers.h"


/*------------------------------------------------------------
    Common Utilities
------------------------------------------------------------*/

COLORREF GetColorForButton(int nButton)
{
    return (nButton == MK_RBUTTON) ? Palette_GetSecondaryColor() : Palette_GetPrimaryColor();
}

BYTE GetOpacityForButton(int nButton)
{
    return (nButton == MK_RBUTTON) ? Palette_GetSecondaryOpacity() : Palette_GetPrimaryOpacity();
}

BYTE ComposeOpacity(BYTE baseAlpha, BYTE colorOpacity)
{
    return (BYTE)(((int)baseAlpha * (int)colorOpacity + 127) / 255);
}

/*------------------------------------------------------------
    Invalidation
------------------------------------------------------------*/



/*------------------------------------------------------------
    Canvas Update Helper - Ensures proper update after pixel modifications
------------------------------------------------------------*/
void UpdateCanvasAfterModification(void)
{
    LayersMarkDirty();
    InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}



/*------------------------------------------------------------
    History Helper Functions
------------------------------------------------------------*/

