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
void PickTool_OnPointer(const ToolPointerEvent *ev) {
	if (!ev)
		return;
	int nButton = (int)ev->buttons;
	if (ev->type == TOOL_POINTER_MOVE && !(nButton & (MK_LBUTTON | MK_RBUTTON)))
		return;
	if (ev->type != TOOL_POINTER_DOWN && ev->type != TOOL_POINTER_MOVE)
		return;
	HandlePickInteraction(ev->bmp.x, ev->bmp.y, nButton);
}
