@echo off
setlocal

:: Find vcvarsall.bat
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%VCVARS%" (
    echo [ERROR] Could not find vcvarsall.bat. Please edit build.bat with the correct path.
    exit /b 1
)

call "%VCVARS%" x64

:: Ensure build directory exists
if not exist "build" mkdir build

:: Compile resources
:: Use full paths to avoid RC1110 errors
rc /nologo /fo"%~dp0build\resource.res" /i "%~dp0include\core" "%~dp0src\resource.rc"

if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

:: Core files
set CORE_FILES=src/main.c src/core/canvas.c src/core/tools.c src/core/draw.c src/core/layers.c src/core/helpers.c src/core/gdi_utils.c src/core/geom.c src/core/file_io.c src/core/history.c src/core/floodfill.c src/core/palette.c src/core/app_commands.c src/core/cursors.c src/core/overlay.c src/core/image_transforms.c src/core/poly_store.c src/core/pixel_ops.c src/core/document.c src/core/controller.c src/core/peztold_core.c src/core/interaction.c src/core/commit_bar.c src/core/tool_session.c

:: Tool files
set TOOL_FILES=src/tools/bezier_tool.c src/tools/brush_presets.c src/tools/crayon_tool.c src/tools/drawing_primitives.c src/tools/fill_tool.c src/tools/freehand_tools.c src/tools/highlighter_tool.c src/tools/magnifier_tool.c src/tools/pen_tool.c src/tools/pick_tool.c src/tools/polygon_tool.c src/tools/selection.c src/tools/shape_tools.c src/tools/text.c

:: UI and Panel files
set UI_FILES=src/ui/widgets/colorbox.c src/ui/widgets/statusbar.c src/ui/widgets/toolbar.c src/ui/panels/history_panel.c src/ui/panels/layers_panel.c

:: Tool Options files
set OPTION_FILES=src/tools/tool_options/presets.c src/tools/tool_options/tool_options.c

:: Libraries
set LIBS=gdi32.lib user32.lib comdlg32.lib shell32.lib msimg32.lib comctl32.lib uxtheme.lib dwmapi.lib ole32.lib windowscodecs.lib uuid.lib oleaut32.lib

:: Include paths
set INCLUDES=/I include /I include/core /I include/tools /I include/tools/tool_options /I include/ui/panels /I include/ui/widgets

:: Compile
cl /nologo /Zi /W3 /D_CRT_SECURE_NO_WARNINGS %INCLUDES% ^
    %CORE_FILES% ^
    %TOOL_FILES% ^
    %UI_FILES% ^
    %OPTION_FILES% ^
    build\resource.res ^
    /Fo:build\ ^
    /Fd:build\ ^
    /Fe:build\PeztoldPaint.exe ^
    /link /SUBSYSTEM:WINDOWS %LIBS%

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] Build completed.
) else (
    echo [ERROR] Build failed with error code %ERRORLEVEL%.
)
