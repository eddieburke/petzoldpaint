#include "cursors.h"
#include "resource.h"
#include "tools.h"
#include "helpers.h"
#include "draw.h"
#include "geom.h"
#include "tools/selection_tool.h"
static void SetAppCursor(int nID) {
	SetCursor(LoadCursor(GetModuleHandle(NULL), MAKEINTRESOURCE(nID)));
}
static void SetPlatformCursor(LPCTSTR lpCursorName) {
	SetCursor(LoadCursor(NULL, lpCursorName));
}
BOOL SetToolCursor(int nToolId, int x, int y) {
	if (IsAltDown())
		nToolId = TOOL_PICK;
	if (nToolId == TOOL_SELECT || nToolId == TOOL_FREEFORM) {
		int nHandle = SelectionGetCursorId(x, y);
		if (nHandle >= HT_ROTATE_TL && nHandle <= HT_ROTATE_BL) {
			SetAppCursor(IDC_CURSOR_ROTATE);
			return TRUE;
		}
		if (nHandle != HT_NONE) {
			SetPlatformCursor(GetHandleCursor(nHandle));
			return TRUE;
		}
	}
	int nAppCursorID = 0;
	switch (nToolId) {
	case TOOL_FREEFORM:
		nAppCursorID = IDC_CURSOR1206;
		break;
	case TOOL_SELECT:
		nAppCursorID = IDC_CURSOR8012;
		break;
	case TOOL_ERASER:
		nAppCursorID = IDC_CURSOR_ERASER;
		break;
	case TOOL_FILL:
		nAppCursorID = IDC_CURSOR1205;
		break;
	case TOOL_PICK:
		nAppCursorID = IDC_CURSOR1204;
		break;
	case TOOL_MAGNIFIER:
		nAppCursorID = IDC_CURSOR1207;
		break;
	case TOOL_PENCIL:
		nAppCursorID = IDC_CURSOR1206;
		break;
	case TOOL_BRUSH:
		nAppCursorID = IDC_CURSOR1201;
		break;
	case TOOL_AIRBRUSH:
		nAppCursorID = IDC_CURSOR8029;
		break;
	case TOOL_TEXT:
		nAppCursorID = IDC_CURSOR8030;
		break;
	case TOOL_LINE:
	case TOOL_CURVE:
	case TOOL_RECT:
	case TOOL_POLYGON:
	case TOOL_ELLIPSE:
	case TOOL_ROUNDRECT:
		nAppCursorID = IDC_CURSOR1201;
		break;
	default:
		break;
	}
	if (nAppCursorID > 0) {
		SetAppCursor(nAppCursorID);
		return TRUE;
	} else if (nToolId == 0) {
		SetPlatformCursor(IDC_ARROW);
		return TRUE;
	}
	SetPlatformCursor(IDC_ARROW);
	return TRUE;
}
