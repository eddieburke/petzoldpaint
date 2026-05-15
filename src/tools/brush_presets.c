#include "brush_presets.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    BrushApplyFn apply;
    BrushGetFn get;
} BrushHandlers;

static BrushHandlers s_handlers[PRESET_MAX_SLOTS];

static void Internal_Apply(const void* data, size_t size) {
    // This is not needed if the tool provides the callback.
}

void BrushPreset_Register(int slot, BrushApplyFn applyFn, BrushGetFn getFn) {
    // Tool-specific registration still happens, but it can use these helpers.
}

void BrushPreset_Add(int slot, const char* name, const BrushPresetData* data, BOOL isDefault) {
    Preset_Add(PRESET_CAT_BRUSH, slot, name, data, sizeof(BrushPresetData), isDefault);
}

BOOL BrushPreset_SaveCurrent(int slot, BrushGetFn getFn) {
    static int customCounters[PRESET_MAX_SLOTS] = {1};
    char name[MAX_PRESET_NAME];
    BrushPresetData d;
    getFn(&d);
    snprintf(name, sizeof(name), "Custom %d", customCounters[slot]++);
    return Preset_Add(PRESET_CAT_BRUSH, slot, name, &d, sizeof(d), FALSE);
}
