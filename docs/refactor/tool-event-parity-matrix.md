# Tool/Event Behavior Parity Matrix (Current Implementation)

This document records **current behavior** for migration parity checks, grouped by tool family and lifecycle event.

Events covered:
- `MouseDown`
- `MouseMove`
- `MouseUp`
- `Cancel`
- `Deactivate`
- `OnCaptureLost`

Legend:
- Capture: `SetCapture` / `ReleaseCapture`
- History: `HistoryPush(...)`
- Dirty flag: `SetDocumentDirty()`
- Redraw: `InvalidateRect(GetCanvasWindow(), ...)`, draft/layer dirty calls
- Button state: stored draw button (`s_nDrawButton`, `drawButton`) and state transitions

## 1) Freehand family (Pencil, Brush, Eraser, Airbrush)

Shared implementation: `FreehandOnMouseDown/Move/Up`, plus Airbrush timer wrapper.

| Event | Behavior |
|---|---|
| MouseDown | If already drawing with different button, calls `CancelFreehandDrawing()` and returns. Otherwise initializes drawing state, stores button/tool/last point, `SetCapture(hWnd)`, draws initial point to active layer, marks dirty rect, invalidates dirty rect on canvas. |
| MouseMove | Runs only while drawing and while mouse button flags are still down; ignores move if active tool changed mid-stroke. Draws interpolated segment/point, marks dirty rect, invalidates rect + full canvas, updates last point. |
| MouseUp | If pixels changed: pushes history using the originally active freehand tool id. If drawing: clears drawing state, `ReleaseCapture()`, calls `SetDocumentDirty()`. |
| Cancel | `CancelFreehandDrawing()` stops airbrush timer if needed, sets drawing false, releases capture, invalidates canvas, **no history push**, **no dirty flag set**. |
| Deactivate | `FreehandTool_Deactivate()` stops timer if needed, and if drawing: clears state, `ReleaseCapture()`, `SetDocumentDirty()`. No history push here. |
| OnCaptureLost | `FreehandTool_OnCaptureLost()` stops timer and pushes history if drawing+pixels modified; `ToolOnCaptureLost()` then invokes `ToolCancel()`, which cancels state/release/invalidate. |

Airbrush-specific addendum:
- `AirbrushToolOnMouseDown` starts timer (ID 101, 30ms) after shared `MouseDown`.
- `AirbrushToolOnMouseUp` kills timer before shared `MouseUp`.
- `Cancel`/`Deactivate`/`OnCaptureLost` paths also kill timer via shared helper.

## 2) Stroke-like tools (Pen, Highlighter, Crayon)

### Pen

| Event | Behavior |
|---|---|
| MouseDown | Starts drawing, stores button/last point, captures mouse, draws initial point, marks layer dirty, invalidates canvas. |
| MouseMove | While drawing + button held: draws line from last point, marks layer dirty, updates last point, invalidates canvas. |
| MouseUp | If modified: push history (`TOOL_PEN`, "Draw"). Then clear drawing, release capture, set document dirty. |
| Cancel | `CancelPenDrawing()` clears drawing, releases capture, invalidates canvas. No history/dirty set. |
| Deactivate | If drawing: clear drawing, release capture, set dirty. No history push. |
| OnCaptureLost | If drawing+modified: pushes history; outer tool lifecycle then performs cancel via `ToolOnCaptureLost` path. |

### Highlighter

Same structure as Pen with highlighter-specific rendering/blend:
- `MouseDown/Move` draw with alpha/blend/softness and mark dirty + invalidate.
- `MouseUp` commits history (`TOOL_HIGHLIGHTER`) if modified, then release + dirty flag.
- `Cancel` releases + invalidate only.
- `Deactivate` releases + dirty flag.
- `OnCaptureLost` pushes history if modified before central cancel path.

### Crayon

| Event | Behavior |
|---|---|
| MouseDown | Initializes noise textures/seed, starts drawing, stores button, captures, resets stroke buffer, adds first point, draws initial spot, marks dirty, invalidates. |
| MouseMove | While drawing + button held: appends stroke points, draws recent smoothed segments, marks dirty if rendered, invalidates. |
| MouseUp | Adds final point, renders final recent segments, may mark dirty, then pushes history if modified, clears drawing, releases capture, sets document dirty, clears stroke points. |
| Cancel | `CancelCrayonDrawing()` clears drawing, releases capture, invalidates, clears stroke points. No history/dirty. |
| Deactivate | If drawing: clears drawing, releases capture, sets dirty, clears stroke points. No history push. |
| OnCaptureLost | Pushes history if drawing+modified; central lifecycle then cancels interaction. |

## 3) Shape family (Line, Rectangle, Ellipse, RoundRect)

Shared implementation in `shape_tools.c` using draft-layer preview + merge on mouse up.

| Event | Behavior |
|---|---|
| MouseDown | If idle: clears draft, enters creating state, stores tool/button/start+end, captures mouse. |
| MouseMove | If active + tool matches: applies shift snapping (line angle or square constraint), updates endpoint, redraws draft (clear+draw), invalidates canvas on change. |
| MouseUp | Releases capture (guarded via `bSuspendingCapture`), computes final snapped end, redraws draft final shape, merges draft to active, marks layer dirty, sets document dirty, pushes history (`"Draw Shape"`), resets state, invalidates canvas. |
| Cancel | `ShapeTool_Cancel()` (unless suspending capture) calls deactivate path: clears draft, resets state, releases capture, invalidates; no history/dirty set. |
| Deactivate | If not idle: clear draft, reset state, release capture, invalidate; no history/dirty set. |
| OnCaptureLost | No tool-specific handler; central `ToolOnCaptureLost()` falls back to `ToolCancel()` so cancel path executes. |

## 4) Pick tool move/down logic

| Event | Behavior |
|---|---|
| MouseDown | Samples composite color at cursor; if valid, applies to primary for left button or secondary for right button; invalidates colorbox window. No capture/history/document dirty/canvas invalidation. |
| MouseMove | Only with button held; continuously samples and updates primary/secondary color; invalidates colorbox window. No capture/history/document dirty. |
| MouseUp | No handler. |
| Cancel / Deactivate / OnCaptureLost | No per-tool hooks. Behavior effectively no-op for picker state. |

## 5) Text tool (modal capture user)

Text modes: `TEXT_NONE`, `TEXT_DRAWING`, `TEXT_EDITING`, `TEXT_RESIZING`.

| Event | Behavior |
|---|---|
| MouseDown | From `TEXT_NONE`: starts drawing box, stores drag start/rect, `SetCapture`. From `TEXT_EDITING`: if handle hit, enters resize and captures; else click outside current box triggers `CommitText(hwnd)`. |
| MouseMove | Requires current capture on hwnd. In drawing mode updates box rect + invalidate canvas. In resizing mode mutates rect/body drag, updates edit position, invalidates canvas. |
| MouseUp | If captured, releases capture. If finishing drawing mode: normalizes/min-sizes rect, creates edit control, applies font, subclasses canvas, switches to editing mode, shows toolbar. If resizing mode: returns to editing mode. Invalidates canvas. |
| Cancel | `CancelText()` clears draft, destroys edit control, hides toolbar, resets mode to none, invalidates canvas; no history/dirty flag. |
| Deactivate | `TextTool_Deactivate()` commits if any mode active (`CommitText(NULL)`). Commit pushes history (`"Text Placement"`) and sets dirty **only if text length > 0 and active bits available**. Always clears draft/editor UI and invalidates. |
| OnCaptureLost | No tool-specific handler in vtable; central cancel may run via lifecycle capture-lost event if runtime capture bookkeeping indicates active capture. |

## 6) Other modal capture users relevant to migration checks

### Selection tool (explicitly modal and capture-heavy)
- Registered as modal (`onDeactivate`, `onCancel`, busy state).
- Uses capture during drag/transform flows.
- On deactivate: commits selection if active (`SelectionTool_Deactivate -> CommitSelection`).
- Included here because tool switching/cancel semantics are central to parity around capture/lifecycle.

### Polygon / Bezier (pending multi-step modal tools)
- Polygon and Bezier use pending states with explicit cancel/deactivate handling.
- Relevant for parity when tool-switching mid-operation and capture-lost paths run central `ToolCancel`.

## 7) Cross-tool lifecycle and edge-case parity criteria

These behaviors are enforced by central tool dispatcher (`tools.c`) and should be preserved:

1. **Capture-lost semantics**
   - `ToolOnCaptureLost()` ignores voluntary release when `s_runtime.capturedWindow == NULL`.
   - For non-voluntary loss: clears runtime capture tracking, calls tool `onCaptureLost` hook (if any), then calls `ToolCancel()` to abort in-progress interaction.

2. **Tool switch mid-draw / modifier-tool swap**
   - Runtime tracks `activeToolAtMouseDown`; move/up dispatch continues to original tool while captured.
   - Freehand specifically rejects move when `s_activeFreehandTool != tool`, protecting against accidental cross-tool continuation.

3. **Cancel pathways**
   - `ToolCancel()` calls tool-specific cancel hook, then ensures capture release, then invalidates canvas.
   - `ToolCancelSkipSelection()` special-cases selection drag finalization before cancellation logic for non-selection tools.

4. **Leaving canvas while drawing**
   - Capture is used by draw tools to continue receiving events outside client bounds.
   - If capture is lost externally, dispatcher routes to capture-lost + cancel behavior above.

5. **Undo/history timing differences (must preserve)**
   - Freehand/pen/highlighter/crayon usually push history on `MouseUp` when pixels changed.
   - Those same tools also push history in `OnCaptureLost` when modified (before cancel).
   - Deactivate/cancel generally do **not** push history (except tools that commit on deactivate like text/selection).

6. **Dirty flag timing differences (must preserve)**
   - Most draw tools call `SetDocumentDirty()` on normal `MouseUp` completion.
   - Cancel generally avoids setting dirty flag.
   - Text sets dirty only when a non-empty text commit actually writes to active bits.

---

## Migration acceptance checklist

Use this document as parity criteria:
- [ ] Capture acquire/release behavior per event remains equivalent.
- [ ] History commit points (MouseUp vs CaptureLost vs Deactivate) stay tool-accurate.
- [ ] Document dirty transitions remain unchanged, including text empty-commit behavior.
- [ ] Invalidate/dirty-rect patterns maintain interactive redraw expectations.
- [ ] Button/state transitions preserve left/right color semantics and in-progress tool identity.
- [ ] Edge cases (capture lost, leaving canvas, tool-switch mid-draw, modifier-triggered pick) match current behavior.
