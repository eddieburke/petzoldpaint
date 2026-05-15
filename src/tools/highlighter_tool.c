/*------------------------------------------------------------------------------
 * HIGHLIGHTER_TOOL.C
 *
 * Highlighter Tool Implementation
 *
 * Implements a highlighter tool using multiply blend mode and alpha blending
 * to simulate a transparent ink effect.
 *----------------------------------------------------------------------------*/

#include "highlighter_tool.h"
#include "canvas.h"
#include "draw.h"
#include "gdi_utils.h"
#include "helpers.h"
#include "layers.h"
#include "resource.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/toolbar.h"
#include "tool_options/presets.h"
#include "tool_options/tool_options.h"
#include "brush_presets.h"
#include <math.h>
#include "interaction.h"
#include <stdio.h>

/* LCG Random Constants */
#define LCG_A 1103515245
#define LCG_C 12345
#define LCG_M 0x7fffffff

/*------------------------------------------------------------------------------
 * Highlighter Presets
 *----------------------------------------------------------------------------*/

static void HighlighterPreset_GetCurrent(BrushPresetData *out) {
  if (!out) return;
  out->highlighter.transparency = nHighlighterTransparency;
  out->highlighter.blendMode = nHighlighterBlendMode;
  out->highlighter.edgeSoftness = nHighlighterEdgeSoftness;
  out->highlighter.opacity = nHighlighterOpacity;
  out->highlighter.sizeVariation = nHighlighterSizeVariation;
  out->highlighter.texture = nHighlighterTexture;
  out->size = nBrushWidth;
}

static void HighlighterPreset_Apply(const void *data, size_t size) {
  if (!data || size != sizeof(BrushPresetData)) return;
  const BrushPresetData *d = (const BrushPresetData *)data;
  nHighlighterTransparency = d->highlighter.transparency;
  nHighlighterBlendMode = d->highlighter.blendMode;
  nHighlighterEdgeSoftness = d->highlighter.edgeSoftness;
  nHighlighterOpacity = d->highlighter.opacity;
  nHighlighterSizeVariation = d->highlighter.sizeVariation;
  nHighlighterTexture = d->highlighter.texture;
  nBrushWidth = d->size;
  SetStoredLineWidth(d->size);
  { HWND h = GetToolOptionsWindow(); if (h) InvalidateRect(h, NULL, FALSE); }
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static BOOL HighlighterPreset_SaveCurrent(void) {
  return BrushPreset_SaveCurrent(PRESET_SLOT_HIGHLIGHTER, (BrushGetFn)HighlighterPreset_GetCurrent);
}

#define REG_PRESET(name, tr, bm, es, op, sv, tx, sz) do { \
    BrushPresetData p = {0}; \
    p.highlighter.transparency = tr; \
    p.highlighter.blendMode = bm; \
    p.highlighter.edgeSoftness = es; \
    p.highlighter.opacity = op; \
    p.highlighter.sizeVariation = sv; \
    p.highlighter.texture = tx; \
    p.size = sz; \
    BrushPreset_Add(PRESET_SLOT_HIGHLIGHTER, (name), &p, TRUE); \
} while(0)

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
     variationSeed = (variationSeed * LCG_A + LCG_C) & LCG_M;
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

  BYTE *bits = LayersGetActiveColorBits();
  if (!bits) {
    Interaction_Abort();
    return;
  }
  BYTE alpha = ComposeOpacity(CalcHighlighterAlpha(), GetOpacityForButton(nButton));
  {
    int sz = GetHighlighterSize();
    int r = sz / 2;
    if (r < 1)
      r = 1;
    DrawCircleAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), x, y, r,
                      GetColorForButton(nButton), alpha, GetHighlighterBlendMode());
  }
  LayersMarkDirty();
  Interaction_MarkModified();
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
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
    {
      int sz = GetHighlighterSize();
      int thick = sz;
      if (thick < 1)
        thick = 1;
      DrawLineAlpha(bits, Canvas_GetWidth(), Canvas_GetHeight(), lp.x, lp.y, x,
                    y, thick,
                    GetColorForButton(Interaction_GetDrawButton()), alpha,
                    GetHighlighterBlendMode());
    }
    LayersMarkDirty();
    Interaction_MarkModified();
  }
  Interaction_UpdateLastPoint(x, y);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
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

