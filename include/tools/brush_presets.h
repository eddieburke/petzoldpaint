#ifndef BRUSH_PRESETS_H
#define BRUSH_PRESETS_H

#include <windows.h>
#include "tool_options/presets.h"

typedef struct {
    union {
        struct {
            int density;
            int textureIntensity;
            int sprayAmount;
            int colorVariation;
            int brightnessRange;
            int saturationRange;
            int hueShiftRange;
        } crayon;
        struct {
            int transparency;
            int blendMode;
            int edgeSoftness;
            int opacity;
            int sizeVariation;
            int texture;
        } highlighter;
    };
    int size;
} BrushPresetData;

typedef void (*BrushGetFn)(BrushPresetData* out);

void BrushPreset_Add(int slot, const char* name, const BrushPresetData* data, BOOL isDefault);
BOOL BrushPreset_SaveCurrent(int slot, BrushGetFn getFn);

#endif
