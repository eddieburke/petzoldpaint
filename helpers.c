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

void InvalidateWindow(HWND hwnd)
{
    if (hwnd) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

void InvalidateCanvas(void)
{
    RefreshCanvasRect(NULL);
}

void InvalidateCanvasRect(RECT* pRect)
{
    RefreshCanvasRect(pRect);
}

/*------------------------------------------------------------
    Canvas Update Helper - Ensures proper update after pixel modifications
------------------------------------------------------------*/
void UpdateCanvasAfterModification(void)
{
    LayersMarkDirty();
    InvalidateCanvas();
}



/*------------------------------------------------------------
    History Helper Functions
------------------------------------------------------------*/

void HistoryPushFormatted(const char* format, ...)
{
    if (!format) return;

    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';

    if (!HistoryPush(buffer)) {
        return;
    }
}

void HistoryPushToolAction(const char* toolName, const char* action)
{
    if (!toolName || !action) return;
    HistoryPushToolActionForActiveLayer(toolName, action);
}

const char* GetToolName(int toolId)
{
    static const char* toolNames[] = {
        [TOOL_FREEFORM] = "Freeform Select",
        [TOOL_SELECT] = "Select",
        [TOOL_ERASER] = "Eraser",
        [TOOL_FILL] = "Fill",
        [TOOL_PICK] = "Pick Color",
        [TOOL_MAGNIFIER] = "Magnifier",
        [TOOL_PENCIL] = "Pencil",
        [TOOL_BRUSH] = "Brush",
        [TOOL_AIRBRUSH] = "Airbrush",
        [TOOL_TEXT] = "Text",
        [TOOL_LINE] = "Line",
        [TOOL_CURVE] = "Curve",
        [TOOL_RECT] = "Rectangle",
        [TOOL_POLYGON] = "Polygon",
        [TOOL_ELLIPSE] = "Ellipse",
        [TOOL_ROUNDRECT] = "Rounded Rectangle",
        [TOOL_PEN] = "Pen",
        [TOOL_HIGHLIGHTER] = "Highlighter",
        [TOOL_CRAYON] = "Crayon"
    };

    if (toolId >= 0 && toolId < (int)(sizeof(toolNames) / sizeof(toolNames[0]))) {
        return toolNames[toolId];
    }
    return "Unknown Tool";
}

void HistoryPushToolActionById(int toolId, const char* action)
{
    HistoryPushToolAction(GetToolName(toolId), action);
}
