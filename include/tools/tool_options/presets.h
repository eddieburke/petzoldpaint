#ifndef PRESETS_H
#define PRESETS_H

#include <windows.h>
#include <stddef.h>


#define MAX_PRESET_NAME 64
#define MAX_PRESETS 32
#define PRESET_MAX_SLOTS 8

typedef enum {
    PRESET_CAT_BRUSH = 0,
    PRESET_CAT_EFFECT = 1,
    PRESET_CAT_LAYER = 2,
    PRESET_CAT_COUNT
} PresetCategory;

/* Slots for BRUSH category: each brush tool gets a slot */
#define PRESET_SLOT_CRAYON      0
#define PRESET_SLOT_HIGHLIGHTER 1

typedef void (*PresetApplyFn)(const void* data, size_t size);
typedef BOOL (*PresetSaveCurrentFn)(void);

/* Register a (category, slot) with apply and save-current callbacks. */
void Preset_RegisterSlot(PresetCategory cat, int slot,
    PresetApplyFn applyFn, PresetSaveCurrentFn saveCurrentFn);

/* Add a preset. isDefault: show as default in menu. */
BOOL Preset_Add(PresetCategory cat, int slot, const char* name,
    const void* data, size_t size, BOOL isDefault);

/* Apply preset at index. */
BOOL Preset_Apply(PresetCategory cat, int slot, int index);

int Preset_GetCount(PresetCategory cat, int slot);
BOOL Preset_GetName(PresetCategory cat, int slot, int index, char* buf, size_t bufSize);

/* Returns: >=0 preset index, -1 cancelled, -2 Save Current. */
int Preset_ShowPopupMenu(HWND hParent, PresetCategory cat, int slot, int x, int y);

/* Invokes the registered save-current for (cat, slot). */
BOOL Preset_SaveCurrent(PresetCategory cat, int slot);

void Preset_SaveAll(void);
void Preset_LoadAll(void);

#endif /* PRESETS_H */
