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

// Highlighter options
extern int nHighlighterTransparency; // 0-255 (0=fully opaque, 255=fully transparent)
extern int nHighlighterBlendMode;    // 0=multiply, 1=screen, 2=overlay
extern int nHighlighterEdgeSoftness; // 0-100
extern int nHighlighterOpacity;      // 0-100
extern int nHighlighterSizeVariation; // 0-100
extern int nHighlighterTexture;      // 0-100

// Crayon options
extern int nCrayonDensity;         // 0-100 (0=sparse, 100=dense)
extern int nCrayonTextureIntensity; // 0-100
extern int nCrayonSprayAmount;     // 0-100
extern int nCrayonColorVariation;  // 0-100
extern int nCrayonBrightnessRange; // 0-100
extern int nCrayonSaturationRange; // 0-100
extern int nCrayonHueShiftRange;   // 0-100
#define SHAPE_BORDER_ONLY   0
#define SHAPE_BORDER_FILL   1
#define SHAPE_SOLID         2
void DrawOptionButtonFrame(HDC hdc, RECT* prc, BOOL bSelected);
void SetStoredLineWidth(int width);
void CreateToolOptions(HWND hParent);
HWND GetToolOptionsWindow(void);
void UpdateToolOptions(int nNewTool);
int GetToolOptionsHeight(void);
#endif
