#ifndef HELPERS_H
#define HELPERS_H

#define _USE_MATH_DEFINES
#include <math.h>
#include <windows.h>


COLORREF GetColorForButton(int nButton);
BYTE GetOpacityForButton(int nButton);
BYTE ComposeOpacity(BYTE baseAlpha, BYTE colorOpacity);

/* Ensure canvas updates properly after pixel modifications */
void UpdateCanvasAfterModification(void);



#endif /* HELPERS_H */
