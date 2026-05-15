/*------------------------------------------------------------------------------
 * HIGHLIGHTER_TOOL.C
 *
 * Highlighter Tool Implementation
 *
 * Implements a highlighter tool using multiply blend mode and alpha blending
 * to simulate a transparent ink effect.
 *----------------------------------------------------------------------------*/

#include "highlighter_tool.h"
#include "../canvas.h"
#include "../draw.h"
#include "../gdi_utils.h"
#include "../helpers.h"
#include "../layers.h"
#include "../resource.h"
#include "../ui/widgets/colorbox.h"
#include "../ui/widgets/toolbar.h"
#include "tool_options/presets.h"
#include "tool_options/tool_options.h"
#include <math.h>
#include "../interaction.h"
#include <stdio.h>

/*------------------------------------------------------------------------------
 * Highlighter Presets
 *----------------------------------------------------------------------------*/

typedef struct {
  int transparency, blendMode, edgeSoftness, opacity, sizeVariation, texture, size;
} HighlighterPresetData;

#define REG_PRESET(name, ...) do { \
    HighlighterPresetData p = {__VA_ARGS__}; \
    Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, (name), &p, sizeof(p), TRUE); \
} while(0)

static void HighlighterPreset_GetCurrent(HighlighterPresetData *out) {
  if (!out) return;
  out->transparency = nHighlighterTransparency;
  out->blendMode = nHighlighterBlendMode;
  out->edgeSoftness = nHighlighterEdgeSoftness;
  out->opacity = nHighlighterOpacity;
  out->sizeVariation = nHighlighterSizeVariation;
  out->texture = nHighlighterTexture;
  out->size = nBrushWidth;
}

static void HighlighterPreset_Apply(const void *data, size_t size) {
  if (!data || size != sizeof(HighlighterPresetData)) return;
  const HighlighterPresetData *d = (const HighlighterPresetData *)data;
  nHighlighterTransparency = d->transparency;
  nHighlighterBlendMode = d->blendMode;
  nHighlighterEdgeSoftness = d->edgeSoftness;
  nHighlighterOpacity = d->opacity;
  nHighlighterSizeVariation = d->sizeVariation;
  nHighlighterTexture = d->texture;
  nBrushWidth = d->size;
  SetStoredLineWidth(d->size);
  { HWND h = GetToolOptionsWindow(); if (h) InvalidateWindow(h); }
  InvalidateCanvas();
}

static BOOL HighlighterPreset_SaveCurrent(void) {
  static int customCounter = 1;
  char name[MAX_PRESET_NAME];
  HighlighterPresetData d;
  HighlighterPreset_GetCurrent(&d);
  StringCchPrintf(name, sizeof(name), "Custom %d", customCounter++);
  return Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, name, &d, sizeof(d), FALSE);
}

void HighlighterTool_RegisterPresets(void) {
  Preset_RegisterSlot(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER,
                      HighlighterPreset_Apply, HighlighterPreset_SaveCurrent);
  REG_PRESET("Standard", 40,0,50,85,20,30,3);
  REG_PRESET("Soft",     60,0,80,70,15,20,2);
  REG_PRESET("Bold",     20,0,30,95,30,50,4);
  REG_PRESET("Neon",     30,1,40,90,25,40,3);
  REG_PRESET("Overlay",  50,2,60,75,20,35,3);
}

/*------------------------------------------------------------
   Highlighter Rendering Helpers
  ------------------------------------------------------------*/

static int GetHighlighterSize(void) {
   int sizes[] = {6, 10, 14, 18, 22};
   int idx = (nBrushWidth - 1) % 5;
   if (idx < 0) idx = 0;
   if (idx >= (int)(sizeof(sizes) / sizeof(sizes[0]))) idx = 0;
   int baseSize = sizes[idx];
   if (nHighlighterSizeVariation > 0) {
     static int variationSeed = 0;
     variationSeed = (variationSeed * 1103515245 + 12345) & 0x7fffffff;
     float variation = ((float)(variationSeed % 200) / 100.0f - 1.0f) *
                       (nHighlighterSizeVariation / 100.0f);
     baseSize = (int)(baseSize * (1.0f + variation * 0.3f));
     if (baseSize < 4) baseSize = 4;
     if (baseSize > 30) baseSize = 30;
   }
   return baseSize;
}

static int GetHighlighterBlendMode(void) {
  static const int blendMap[] = { LAYER_BLEND_MULTIPLY, LAYER_BLEND_SCREEN, LAYER_BLEND_OVERLAY };
  return (nHighlighterBlendMode >= 0 && nHighlighterBlendMode < 3) ? blendMap[nHighlighterBlendMode] : LAYER_BLEND_MULTIPLY;
}

static BYTE CalcHighlighterAlpha(void) {
  BYTE baseAlpha = (BYTE)max(0, min(255, 255 - nHighlighterTransparency));
  return (BYTE)(baseAlpha * ((float)nHighlighterOpacity / 100.0f));
}

/*------------------------------------------------------------
   Highlighter Tool Public API
  ------------------------------------------------------------*/

void HighlighterToolOnMouseDown(HWND hWnd, int x, int y, int nButton) {
  Interaction_Begin(hWnd, x, y, nButton, TOOL_HIGHLIGHTER);

  BYTE alpha = ComposeOpacity(CalcHighlighterAlpha(), GetOpacityForButton(nButton));
  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    DrawCircleAAAlphaSoft(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y,
                          GetHighlighterSize() / 2.0f, GetColorForButton(nButton),
                          alpha, GetHighlighterBlendMode(),
                          (float)nHighlighterEdgeSoftness / 100.0f);
    LayersMarkDirty();
    Interaction_MarkModified();
  }
  InvalidateCanvas();
}

void HighlighterToolOnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!Interaction_IsActive() || !Interaction_IsActiveButton(nButton))
    return;

  BYTE alpha = ComposeOpacity(CalcHighlighterAlpha(), GetOpacityForButton(Interaction_GetDrawButton()));
  BYTE *bits = LayersGetActiveColorBits();
  if (bits) {
    POINT lp;
    Interaction_GetLastPoint(&lp);
    DrawLineAAAlphaSoft(bits, Canvas_GetWidth(), Canvas_GetHeight(),
                        (float)lp.x, (float)lp.y,
                        (float)x, (float)y, GetHighlighterSize() / 2.0f,
                        GetColorForButton(Interaction_GetDrawButton()), alpha,
                        GetHighlighterBlendMode(),
                        (float)nHighlighterEdgeSoftness / 100.0f);
    LayersMarkDirty();
    Interaction_MarkModified();
  }
  Interaction_UpdateLastPoint(x, y);
  InvalidateCanvas();
}

void HighlighterToolOnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  Interaction_Commit("Draw");
}

BOOL IsHighlighterDrawing(void) { return Interaction_IsActive(); }

void HighlighterTool_Deactivate(void) { Interaction_EndQuiet(); }

BOOL CancelHighlighterDrawing(void) {
  if (!Interaction_IsActive())
    return FALSE;
  Interaction_Abort();
  return TRUE;
}

