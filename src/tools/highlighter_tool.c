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
#include <math.h>
#include <stdio.h>
#include "interaction.h"

#define LCG_A 1103515245
#define LCG_C 12345
#define LCG_M 0x7fffffff

typedef struct {
  int transparency, blendMode, edgeSoftness;
  int opacity, sizeVariation, texture;
  int size;
} HighlighterPresetData;

static void HighlighterPreset_Apply(const void *data, size_t sz) {
  if (!data || sz != sizeof(HighlighterPresetData)) return;
  const HighlighterPresetData *d = (const HighlighterPresetData *)data;
  nHighlighterTransparency = d->transparency;
  nHighlighterBlendMode    = d->blendMode;
  nHighlighterEdgeSoftness = d->edgeSoftness;
  nHighlighterOpacity      = d->opacity;
  nHighlighterSizeVariation= d->sizeVariation;
  nHighlighterTexture      = d->texture;
  nBrushWidth              = d->size;
  SetStoredLineWidth(d->size);
  HWND h = GetToolOptionsWindow();
  if (h) InvalidateRect(h, NULL, FALSE);
  InvalidateRect(GetCanvasWindow(), NULL, FALSE);
}

static BOOL HighlighterPreset_SaveCurrent(void) {
  static int s_customCount = 1;
  HighlighterPresetData d = {
    nHighlighterTransparency, nHighlighterBlendMode, nHighlighterEdgeSoftness,
    nHighlighterOpacity, nHighlighterSizeVariation, nHighlighterTexture, nBrushWidth
  };
  char name[MAX_PRESET_NAME];
  snprintf(name, sizeof(name), "Custom %d", s_customCount);
  if (!Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, name, &d, sizeof(d), FALSE))
    return FALSE;
  s_customCount++;
  return TRUE;
}

void HighlighterTool_RegisterPresets(void) {
  Preset_RegisterSlot(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER,
                      HighlighterPreset_Apply, HighlighterPreset_SaveCurrent);
#define REG(name, tr, bm, es, op, sv, tx, sz) do { \
    HighlighterPresetData p = {tr,bm,es,op,sv,tx,sz}; \
    Preset_Add(PRESET_CAT_BRUSH, PRESET_SLOT_HIGHLIGHTER, name, &p, sizeof(p), TRUE); \
} while(0)
  REG("Standard", 40,0,50,85,20,30,3);
  REG("Soft",     60,0,80,70,15,20,2);
  REG("Bold",     20,0,30,95,30,50,4);
  REG("Neon",     30,1,40,90,25,40,3);
  REG("Overlay",  50,2,60,75,20,35,3);
#undef REG
}


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


void HighlighterTool_OnMouseDown(HWND hWnd, int x, int y, int nButton) {
  Interaction_Begin(hWnd, x, y, nButton, TOOL_HIGHLIGHTER);

  BYTE *bits = LayersGetStrokeBits();
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
  Interaction_MarkModified();
  {
    int pad = GetHighlighterSize() + 2;
    Interaction_NoteStrokeSegment(x, y, x, y, pad);
    Interaction_FlushStrokeRedraw();
  }
}

void HighlighterTool_OnMouseMove(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  if (!Interaction_IsActive() || !Interaction_IsActiveButton(nButton))
    return;

  BYTE alpha = ComposeOpacity(CalcHighlighterAlpha(), GetOpacityForButton(Interaction_GetDrawButton()));
  BYTE *bits = LayersGetStrokeBits();
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
    Interaction_MarkModified();
    Interaction_NoteStrokeSegment(lp.x, lp.y, x, y, GetHighlighterSize() + 2);
  }
  Interaction_UpdateLastPoint(x, y);
}

void HighlighterTool_OnMouseUp(HWND hWnd, int x, int y, int nButton) {
  (void)hWnd;
  (void)x;
  (void)y;
  (void)nButton;
  Interaction_Commit("Draw");
}

BOOL IsHighlighterDrawing(void) {
  return Interaction_IsActive() &&
         Interaction_GetActiveToolId() == TOOL_HIGHLIGHTER;
}

void HighlighterTool_Deactivate(void) {
  if (Interaction_IsActive() &&
      Interaction_GetActiveToolId() == TOOL_HIGHLIGHTER)
    Interaction_EndQuiet();
}

BOOL HighlighterTool_Cancel(void) {
  if (!Interaction_IsActive() ||
      Interaction_GetActiveToolId() != TOOL_HIGHLIGHTER)
    return FALSE;
  Interaction_Abort();
  return TRUE;
}

