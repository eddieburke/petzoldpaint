#ifndef TOOL_OPTIONS_H
#define TOOL_OPTIONS_H
#include <windows.h>

#define TOOL_OPTIONS_HEIGHT  400
#define OPTION_GAP      2
#define OPTIONS_MARGIN  4
#define SELECTION_OPAQUE      0
#define SELECTION_TRANSPARENT 1
extern int nSelectionMode;
extern int nBrushWidth;
extern int nSprayRadius;
extern int nShapeDrawType;

extern int nHighlighterTransparency;
extern int nHighlighterBlendMode;
extern int nHighlighterEdgeSoftness;
extern int nHighlighterOpacity;
extern int nHighlighterSizeVariation;
extern int nHighlighterTexture;

extern int nCrayonDensity;
extern int nCrayonTextureIntensity;
extern int nCrayonSprayAmount;
extern int nCrayonColorVariation;
extern int nCrayonBrightnessRange;
extern int nCrayonSaturationRange;
extern int nCrayonHueShiftRange;
#define SHAPE_BORDER_ONLY   0
#define SHAPE_BORDER_FILL   1
#define SHAPE_SOLID         2
void DrawOptionButtonFrame(HDC hdc, RECT* prc, BOOL bSelected);
void SetStoredLineWidth(int width);
void CreateToolOptions(HWND hParent);
HWND GetToolOptionsWindow(void);
void UpdateToolOptions(int nNewTool);
#endif
