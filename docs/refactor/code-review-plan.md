# Code Review Plan Sheet - PeztoldPaint Refactor

## Review Scope
All files modified during the history/layers/selection/interaction/freehand/palette refactor.

## Agent Assignment

### Agent 1 - Core Systems
Files: history.c/h, layers.c/h, interaction.c/h, palette.c/h

### Agent 2 - Tools & UI
Files: selection.c/h, freehand_tools.c/h, colorbox.c/h, controller.c, tools.c

## Review Checklist Per File

### 1. API Compatibility
- [ ] All exported functions match their header declarations
- [ ] Function signatures unchanged where called by other modules
- [ ] No missing includes for functions used from other files

### 2. Memory Safety
- [ ] No memory leaks (malloc without free, missing cleanup on error paths)
- [ ] No use-after-free
- [ ] No double-free
- [ ] Proper NULL checks before dereference
- [ ] Buffer overflows (strcpy vs strncpy_s, array bounds)

### 3. Logic Errors
- [ ] Off-by-one errors in loops
- [ ] Division by zero
- [ ] Uninitialized variables
- [ ] State machine transitions correct
- [ ] Edge cases handled (empty, zero-size, NULL)

### 4. Refactor-Specific Risks
- [ ] History: Full snapshots work correctly for undo/redo
- [ ] History: Tool session snapshots still captured properly
- [ ] Layers: Compositing produces correct output
- [ ] Layers: Draft layer merges correctly
- [ ] Layers: Snapshot create/apply round-trip works
- [ ] Selection: Rotation sampling correct
- [ ] Selection: Lift/commit/cancel flow intact
- [ ] Selection: Clipboard copy/paste works
- [ ] Interaction: Begin/commit/abort lifecycle correct
- [ ] Freehand: Drawing interpolation correct
- [ ] Freehand: Airbrush timer works
- [ ] Palette: Color picker dialog opens/closes correctly
- [ ] Palette: RGBA conversions correct

### 5. Build Warnings
- [ ] C4013: Function undefined (missing include/declaration)
- [ ] C4047: Type mismatches
- [ ] C4996: Deprecated functions (strdup)
- [ ] C4029: Parameter list mismatches

### 6. Pre-existing Issues (not introduced by refactor)
- [ ] Any bugs that existed before and are still present
- [ ] Performance issues
- [ ] Race conditions
- [ ] Resource leaks

## Priority Levels
- **P0**: Crash or data loss - must fix immediately
- **P1**: Feature broken - must fix before release
- **P2**: Warning or minor issue - should fix
- **P3**: Code quality - nice to fix

## Output Format
Each agent should return:
1. File-by-file findings with line numbers
2. Severity classification (P0-P3)
3. Whether bug is NEW (introduced by refactor) or EXISTING
4. Suggested fix
