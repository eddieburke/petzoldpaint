#include "presets.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <strsafe.h>

/*------------------------------------------------------------
   Slot registration: apply and save-current per (cat, slot)
  ------------------------------------------------------------*/

typedef struct {
    PresetApplyFn apply;
    PresetSaveCurrentFn saveCurrent;
} SlotReg;

static SlotReg s_slotReg[PRESET_CAT_COUNT][PRESET_MAX_SLOTS];

/*------------------------------------------------------------
   Preset entry storage
  ------------------------------------------------------------*/

typedef struct {
    char name[MAX_PRESET_NAME];
    BOOL isDefault;
    void* data;
    size_t size;
} PresetEntry;

static PresetEntry s_entries[PRESET_CAT_COUNT][PRESET_MAX_SLOTS][MAX_PRESETS];
static int s_counts[PRESET_CAT_COUNT][PRESET_MAX_SLOTS];

/*------------------------------------------------------------
   API Implementation
  ------------------------------------------------------------*/

void Preset_RegisterSlot(PresetCategory cat, int slot,
    PresetApplyFn applyFn, PresetSaveCurrentFn saveCurrentFn)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return;
    s_slotReg[cat][slot].apply = applyFn;
    s_slotReg[cat][slot].saveCurrent = saveCurrentFn;
}

BOOL Preset_Add(PresetCategory cat, int slot, const char* name,
    const void* data, size_t size, BOOL isDefault)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return FALSE;
    if (!name || !data) return FALSE;
    int c = s_counts[cat][slot];
    if (c >= MAX_PRESETS) return FALSE;

    PresetEntry* e = &s_entries[cat][slot][c];
    StringCchCopy(e->name, MAX_PRESET_NAME, name);
    e->isDefault = isDefault;
    e->data = malloc(size);
    if (!e->data) return FALSE;
    memcpy(e->data, data, size);
    e->size = size;
    s_counts[cat][slot]++;
    return TRUE;
}

BOOL Preset_Apply(PresetCategory cat, int slot, int index)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return FALSE;
    if (index < 0 || index >= s_counts[cat][slot]) return FALSE;
    PresetApplyFn fn = s_slotReg[cat][slot].apply;
    if (!fn) return FALSE;
    PresetEntry* e = &s_entries[cat][slot][index];
    fn(e->data, e->size);
    return TRUE;
}

int Preset_GetCount(PresetCategory cat, int slot)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return 0;
    return s_counts[cat][slot];
}

BOOL Preset_GetName(PresetCategory cat, int slot, int index, char* buf, size_t bufSize)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return FALSE;
    if (index < 0 || index >= s_counts[cat][slot] || !buf || bufSize == 0) return FALSE;
    StringCchCopy(buf, bufSize, s_entries[cat][slot][index].name);
    return TRUE;
}

int Preset_ShowPopupMenu(HWND hParent, PresetCategory cat, int slot, int x, int y)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return -1;

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return -1;

    int count = Preset_GetCount(cat, slot);
    for (int i = 0; i < count; i++) {
        PresetEntry* e = &s_entries[cat][slot][i];
        UINT flags = MF_STRING;
        if (e->isDefault) flags |= MF_DEFAULT;
        AppendMenu(hMenu, flags, 1000 + i, e->name);
    }

    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, 2000, "Save Current");

    POINT pt = {x, y};
    ClientToScreen(hParent, &pt);

    int cmd = (int)TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                 pt.x, pt.y, 0, hParent, NULL);

    DestroyMenu(hMenu);

    if (cmd >= 1000 && cmd < 1000 + count)
        return cmd - 1000;
    if (cmd == 2000)
        return -2;
    return -1;
}

BOOL Preset_SaveCurrent(PresetCategory cat, int slot)
{
    if (cat >= PRESET_CAT_COUNT || slot >= PRESET_MAX_SLOTS) return FALSE;
    PresetSaveCurrentFn fn = s_slotReg[cat][slot].saveCurrent;
    if (!fn) return FALSE;
    return fn();
}

void Preset_SaveAll(void)
{
    /* TODO: persist to file */
}

void Preset_LoadAll(void)
{
    /* TODO: load from file; must be called after all Preset_RegisterSlot. */
}
