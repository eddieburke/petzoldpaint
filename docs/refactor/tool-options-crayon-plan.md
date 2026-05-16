# Tool options split and crayon simplification plan

## Goals

- Split `tool_options.c` (~1000 lines) into focused modules without changing behavior.
- Simplify `crayon_tool.c` (~600 lines) while keeping the same visual output and preset names.
- Keep build wiring in `build.bat` explicit (one `.c` per module).

## tool_options.c separation (proposed layout)

| New file | Responsibility |
| -------- | -------------- |
| `tool_options/tool_options_core.c` | Window proc, layout, `GetToolOptionsWindow`, tool activation routing |
| `tool_options/brush_options.c` | Pencil/brush/eraser/airbrush size and opacity UI |
| `tool_options/shape_options.c` | Line/fill style, shape stroke width |
| `tool_options/zoom_options.c` | Magnifier zoom presets (already partially isolated) |
| `tool_options/crayon_options.c` | Crayon density/texture sliders (callbacks stay thin; logic in crayon_tool) |
| `tool_options/presets.c` | Unchanged slot registry |
| `tool_options/tool_options.h` | Shared panel API + `ToolOptionsPanel` vtable |

**Migration order:** extract zoom + brush first (low coupling), then shape, then crayon UI, leave core proc last.

## crayon_tool.c simplification (behavior-preserving)

1. **Preset registration** — one helper `CrayonPreset_RegisterNamed(name, BrushPresetData *tpl)` instead of repeated field copies.
2. **Stroke drawing** — keep `DrawCrayonStrokeSmooth` but move LCG/color variation into `crayon_stroke.c` (~200 lines) so `crayon_tool.c` is input + Interaction only.
3. **Shared with brush** — crayon and brush already share `nBrushWidth`; document that crayon_options only forwards to globals in `tool_options.c`.

**Do not change:** preset names, default values, timer/interaction flow, or spline segment count (visual parity).

## Dead code removed (this pass)

- `canvas.c` `Undo()` / `Redo()` wrappers — callers use `history.h` directly.
- Duplicate `magnifier_tool.h` include in `tools.c` (prior pass).

## Verification

- Build `build.bat` after each extraction step.
- Manual: crayon soft/medium presets, brush width, shape fill, zoom panel.
- Regression: stroke cancel (Escape) clears draft; selection arrow nudge; undo depth ≤ 50 steps.
