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

:: Compile resources
:: Use full paths to avoid RC1110 errors
rc /nologo /fo"%~dp0resource.res" "%~dp0resource.rc"

if errorlevel 1 (
    echo [ERROR] Resource compilation failed.
    exit /b 1
)

:: Core files
set CORE_FILES=main.c canvas.c tools.c draw.c layers.c helpers.c gdi_utils.c geom.c file_io.c history.c floodfill.c palette.c app_commands.c cursors.c overlay.c image_transforms.c poly_store.c pixel_ops.c document.c controller.c


:: Tool files
set TOOL_FILES=tools/bezier_tool.c tools/crayon_tool.c tools/drawing_primitives.c tools/fill_tool.c tools/freehand_tools.c tools/highlighter_tool.c tools/magnifier_tool.c tools/pen_tool.c tools/pick_tool.c tools/polygon_tool.c tools/selection.c tools/shape_tools.c tools/stroke_session.c tools/text.c

:: UI and Panel files
set UI_FILES=ui/widgets/colorbox.c ui/widgets/statusbar.c ui/widgets/toolbar.c ui/panels/history_panel.c ui/panels/layers_panel.c

:: Tool Options files
set OPTION_FILES=tools/tool_options/presets.c tools/tool_options/tool_options.c

:: Libraries
set LIBS=gdi32.lib user32.lib comdlg32.lib shell32.lib msimg32.lib comctl32.lib uxtheme.lib dwmapi.lib ole32.lib windowscodecs.lib uuid.lib oleaut32.lib

:: Compile
cl /nologo /Zi /W3 /D_CRT_SECURE_NO_WARNINGS /I. ^
    %CORE_FILES% ^
    %TOOL_FILES% ^
    %UI_FILES% ^
    %OPTION_FILES% ^
    resource.res ^
    /Fe:PeztoldPaint.exe ^
    /link /SUBSYSTEM:WINDOWS %LIBS%

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] Build completed.
) else (
    echo [ERROR] Build failed with error code %ERRORLEVEL%.
)
