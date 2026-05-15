#include "brush_presets.h"
#include <stdio.h>

void BrushPreset_Add(int slot, const char* name, const BrushPresetData* data, BOOL isDefault) {
    Preset_Add(PRESET_CAT_BRUSH, slot, name, data, sizeof(BrushPresetData), isDefault);
}

BOOL BrushPreset_SaveCurrent(int slot, BrushGetFn getFn) {
    static int customCounters[PRESET_MAX_SLOTS];
    char name[MAX_PRESET_NAME];
    BrushPresetData d;
    if (customCounters[slot] <= 0)
        customCounters[slot] = 1;
    getFn(&d);
    snprintf(name, sizeof(name), "Custom %d", customCounters[slot]);
    if (!Preset_Add(PRESET_CAT_BRUSH, slot, name, &d, sizeof(d), FALSE))
        return FALSE;
    customCounters[slot]++;
    return TRUE;
}
