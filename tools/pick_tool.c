/*------------------------------------------------------------------------------
 * PICK_TOOL.C
 *
 * Color Picker Tool Implementation
 *
 * Implements the color picker (dropper) tool, which samples colors from the
 * composite canvas image and updates the active foreground or background color.
 *----------------------------------------------------------------------------*/

#include "pick_tool.h"
#include "../canvas.h"
#include "../helpers.h"
#include "../layers.h"
#include "../ui/widgets/colorbox.h"
#include "../palette.h"

/*------------------------------------------------------------------------------
 * Pick Tool Public API
 *----------------------------------------------------------------------------*/

void PickToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  COLORREF color = LayersSampleCompositeColor(x, y, Palette_GetSecondaryColor());
  if (color == CLR_INVALID)
    return;

  if (nButton & MK_LBUTTON) {
    Palette_SetPrimaryColor(color);
  } else if (nButton & MK_RBUTTON) {
    Palette_SetSecondaryColor(color);
  }

  // Also handle cases where nButton might be passed as a specific ID instead of
  // flags (though standard WM_ messages use flags) Logic above assumes nButton
  // contains MK_ flags.

  InvalidateWindow(GetColorboxWindow());
}

void PickToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  if (!(nButton & (MK_LBUTTON | MK_RBUTTON)))
    return;

  COLORREF color = LayersSampleCompositeColor(x, y, Palette_GetSecondaryColor());
  if (color == CLR_INVALID)
    return;

  if (nButton & MK_LBUTTON) {
    Palette_SetPrimaryColor(color);
  } else {
    Palette_SetSecondaryColor(color);
  }
  InvalidateWindow(GetColorboxWindow());
}
