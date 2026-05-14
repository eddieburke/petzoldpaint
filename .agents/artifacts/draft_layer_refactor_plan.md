# Draft Layer / Tool Lifecycle Refactoring Plan

## Status: IN PROGRESS

## Current Architecture Summary
- **ToolVTable** has: `onMouseDown`, `onMouseMove`, `onMouseUp`, `onDoubleClick`, `drawGhost`, `drawOverlay`
- **Ghost drawing**: Tools provide a `drawGhost(HDC)` callback rendered into a temp preview buffer during `WM_PAINT`
- **Commit flow**: `ToolCommitGeneric()` obtains active layer DC, calls `drawGhost(layerDC)`, marks dirty, cleans up
- **`CommitCurrentTool()`** checks each tool's pending state and calls appropriate commit
- **UI coupling**: `layers_panel.c`, `history_panel.c`, and `SetCurrentTool()` all manually call `CommitCurrentTool()`
- **Selection tool**: Uses `BackupData` struct for canvas backup/restore during floating selections
- **`canvas_preview.c`**: Separate cached preview buffer system used only in `WM_PAINT`

## Phase 1: Draft Layer in Compositor

### Goal
Add a `s_draftLayer` to `layers.c` that renders between the active layer and upper layers.

### Files Modified
- `layers.h` — Add Draft Layer API declarations
- `layers.c` — Add `s_draftLayer`, modify `CompositeLayersToBufferUnified()`, add API functions

### New API Functions
```c
BYTE* LayersGetDraftBits(void);           // Returns draft layer pixel buffer
void  LayersClearDraft(void);             // Zeroes draft layer (alpha=0)
void  LayersMergeDraftToActive(void);     // Blends draft into active layer, clears draft
BOOL  LayersIsDraftDirty(void);           // Check if draft has content
void  LayersEnsureDraft(void);            // Ensure draft layer allocated
```

### Compositor Change
In `CompositeLayersToBufferUnified()`, after processing layer `i == s_activeIndex`, blend the draft layer before continuing to the next layer above.

---

## Phase 2: Tool VTable Lifecycle

### Goal
Add `onActivate`, `onDeactivate`, `onCancel` to the VTable. Create `Tool_FinalizeCurrentState()`.

### Files Modified
- `tools.c` — Expand `ToolVTable` struct, update table entries, add lifecycle dispatch
- `tools.h` — Update public API: add `Tool_FinalizeCurrentState()`, remove `CommitCurrentTool()` (eventually)

### New VTable Fields
```c
void (*onActivate)(void);
void (*onDeactivate)(void);
BOOL (*onCancel)(void);
```

### Key Changes
- `SetCurrentTool()`: Call `onDeactivate()` on old tool, `onActivate()` on new tool
- `Tool_FinalizeCurrentState()`: Calls current tool's `onDeactivate()` — replaces scattered `CommitCurrentTool()` calls
- Replace all `CommitCurrentTool()` calls in `layers_panel.c`, `history_panel.c`, `app_commands.c` with `Tool_FinalizeCurrentState()`

---

## Phase 3: Refactor Shape Tools

### Goal
Shapes draw to draft layer instead of ghost. Fire-and-forget on MouseUp.

### Files Modified
- `tools/shape_tools.c` — Rewrite to use Draft Layer
- `tools/shape_tools.h` — Remove DrawGhost declarations

### Delete
- `DrawCurrentShape(HDC hdc)` — ghost version
- `ShapeToolDrawGhost()` and per-shape `*DrawGhost` wrappers
- `CommitShape()` no longer calls `ToolCommitGeneric`, instead calls `LayersMergeDraftToActive()`
- `CommitPendingShape()` / `IsShapePending()` become simpler or unnecessary

### New Flow
1. **MouseDown**: Note start, `LayersClearDraft()`
2. **MouseMove**: `LayersClearDraft()`, draw shape to draft bits
3. **MouseUp**: `LayersMergeDraftToActive()`, push history, reset
4. **onDeactivate**: `LayersClearDraft()`, reset state
5. **onCancel**: `LayersClearDraft()`, reset state

---

## Phase 4: Refactor Polygon & Bezier Tools

### Files Modified
- `tools/polygon_tool.c` — Draw to draft layer
- `tools/bezier_tool.c` — Draw to draft layer

### Delete
- `PolygonTool_DrawGhost()` / `PolygonCommitCallback()` / `CommitPendingPolygon()`
- `BezierGhostDraw()` / `BezierCommitCallback()` / `CommitPendingCurve()`
- Remove `ToolCommitGeneric` calls

### New Flow (Polygon)
1. **MouseDown/Move**: Draw open polygon + rubber band to **Draft Layer**
2. **DoubleClick/Close**: `LayersMergeDraftToActive()`, push history
3. **onDeactivate/onCancel**: `LayersClearDraft()`, reset

### New Flow (Bezier)
1. **Each state**: Draw curve preview to **Draft Layer**
2. **Final MouseUp (DRAG_CTRL2)**: `LayersMergeDraftToActive()`, push history
3. **onDeactivate/onCancel**: `LayersClearDraft()`, reset

---

## Phase 5: Refactor Text Tool

### Files Modified
- `tools/text_tool.c` — Remove TextToolDrawGhost

### Delete
- `TextToolDrawGhost()` — replaced by the Windows Edit control rendering

### New Flow
- **MouseDown**: User drags text box, show edit control
- **Typing**: Edit control handles visual – no ghost needed
- **Click Outside / onDeactivate**: Render text to **active layer** via `TextRender_ToBuffer`, push history
- **onCancel**: Hide edit control, no canvas change

---

## Phase 6: Refactor Selection Tool (eventual)

### Files Modified
- `tools/selection_tool.c` — Use draft layer for floating pixels

### Delete
- `BackupData` struct
- `SelectionHelpers_BackupCanvas()` / `SelectionHelpers_RestoreCanvas()`
- `SelectionToolDraw()` (compositor handles draft layer)

### New Flow
- **Phase A**: Marching ants only (RECT/HRGN stored), no pixel changes
- **Phase B (Lift)**: Copy pixels to draft layer, erase from active
- **Phase C (Float)**: Move/transform draft layer content
- **Commit**: `LayersMergeDraftToActive()`
- **Cancel**: Restore erased pixels from history

---

## Phase 7: Cleanup

### Delete Entirely
- `canvas_preview.c` / `canvas_preview.h` — draft layer replaces this
- `ToolCommitGeneric()` — no longer needed
- `CommitCurrentTool()` — replaced by `Tool_FinalizeCurrentState()`
- `CommitPendingShapes()`, `CommitCurrentSelection()`
- All `*DrawGhost` functions across all tools
- Remove `drawGhost` from VTable

### Update
- `canvas.c` WM_PAINT: Remove `ToolDrawGhost()` call and preview buffer code
- `build.bat`: Remove `canvas_preview.c`

---

## Execution Order
We will implement phases sequentially, building and testing after each phase.
Each phase should result in a compilable, working state.
