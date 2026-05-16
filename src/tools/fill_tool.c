#include "fill_tool.h"
#include "peztold_core.h"
#include "floodfill.h"
#include "helpers.h"
#include "history.h"
#include "canvas.h"
void FillTool_OnPointer(const ToolPointerEvent *ev) {
	if (!ev || ev->type != TOOL_POINTER_DOWN)
		return;
	int x = ev->bmp.x;
	int y = ev->bmp.y;
	int nButton = (int)ev->buttons;
	if (nButton != MK_LBUTTON && nButton != MK_RBUTTON)
		return;
	if (FloodFillCanvas(x, y, GetColorForButton(nButton), GetOpacityForButton(nButton))) {
		HistoryPush("Flood Fill");
		InvalidateRect(GetCanvasWindow(), NULL, FALSE);
		SetDocumentDirty();
	}
}
