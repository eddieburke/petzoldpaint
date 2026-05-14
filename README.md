# Peztold Paint

A GDI-based painting application for Windows, inspired by classic paint programs. Built with the Win32 API and MSVC.

## Features

- Layered canvas with full layer management
- Drawing tools: pen, brush, crayon, highlighter, bezier, polygon, shape tools, text, flood fill, pick tool
- Selection tool with move/cut/copy/delete
- Freehand selection
- Image transforms (scale, skew, rotate, flip, invert)
- Magnifier zoom (12.5% - 3200%)
- Color palette with customizable colors
- Undo/redo history (stores up to 100 most recent actions)
- Tool presets
- Drag-and-drop file opening
- Status bar, color box, tool options panels

## Building

Requires Visual Studio (2022, 2019, or 2017). Run:

```
build.bat
```

The script will find `vcvarsall.bat` and compile with MSVC. Output: `PeztoldPaint.exe`.

## Controls

- **Canvas**: Left-click to draw with selected tool, right-click for context menu
- **Toolbar**: Vertical toolbar on the left to select tools
- **Layers Panel**: Add, remove, reorder layers
- **History Panel**: Undo/redo actions
- **Color Box**: Choose primary/secondary colors
- **Status Bar**: Shows tool info and coordinates
