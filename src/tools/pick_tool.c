#include "pick_tool.h"
#include "canvas.h"
#include "helpers.h"
#include "layers.h"
#include "ui/widgets/colorbox.h"
#include "palette.h"


static COLORREF SampleColorAtPoint(int x, int y) {
  return LayersSampleCompositeColor(x, y);
}

static void ApplyPickedColor(COLORREF color, int nButton) {
  if (nButton & MK_LBUTTON) {
    Palette_SetPrimaryColor(color);
  } else if (nButton & MK_RBUTTON) {
    Palette_SetSecondaryColor(color);
  }

  InvalidateRect(GetColorboxWindow(), NULL, FALSE);
}

static void HandlePickInteraction(int x, int y, int nButton) {
  COLORREF color = SampleColorAtPoint(x, y);
  if (color == CLR_INVALID)
    return;

  ApplyPickedColor(color, nButton);
}


void PickTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;

  // Also handle cases where nButton might be passed as a specific ID instead of
  // flags (though standard WM_ messages use flags).
  HandlePickInteraction(x, y, nButton);
}

void PickTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;

  if (!(nButton & (MK_LBUTTON | MK_RBUTTON)))
    return;

  HandlePickInteraction(x, y, nButton);
}
