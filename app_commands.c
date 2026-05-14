/*------------------------------------------------------------
   app_commands.c - Menu command and init-menu-popup handlers
------------------------------------------------------------*/

#include "peztold_core.h"
#include "canvas.h"
#include "controller.h"
#include "file_io.h"
#include "history.h"
#include "image_transforms.h"
#include "layers.h"

#include "tools/selection_tool.h"
#include "tools/text_tool.h"
#include "tools/tool_options/tool_options.h"
#include "ui/panels/layers_panel.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/statusbar.h"
#include "tools.h"

extern void ResizeLayout(HWND hwnd);

/*------------------------------------------------------------
   Zoom Menu Helper
------------------------------------------------------------*/

typedef struct {
  WORD id;
  double zoom;
} ZoomMenuItem;

static BOOL HandleZoomMenu(HWND hwnd, WORD id) {
  static const ZoomMenuItem kZoomItems[] = {
      {IDM_ZOOM_12, 12.5},   {IDM_ZOOM_25, 25.0},   {IDM_ZOOM_50, 50.0},
      {IDM_ZOOM_100, 100.0}, {IDM_ZOOM_200, 200.0}, {IDM_ZOOM_400, 400.0},
      {IDM_ZOOM_800, 800.0}};

  for (int i = 0; i < (int)(sizeof(kZoomItems) / sizeof(kZoomItems[0])); i++) {
    if (kZoomItems[i].id == id) {
      Canvas_SetZoom(kZoomItems[i].zoom);
      ResizeLayout(hwnd);
      InvalidateCanvas();
      return TRUE;
    }
  }
  return FALSE;
}

static BOOL CanPasteFromClipboard(void) {
  UINT uPng = RegisterClipboardFormatA("PNG");
  if (uPng && IsClipboardFormatAvailable(uPng))
    return TRUE;
  return IsClipboardFormatAvailable(CF_DIBV5) ||
         IsClipboardFormatAvailable(CF_BITMAP);
}

static void SyncAfterDocumentLoadOrReset(HWND hwnd) {
  ResetCanvasScroll();
  HWND hCv = GetCanvasWindow();
  if (hCv)
    Controller_UpdateScrollbars(hCv);
  LayersPanelSync();
  InvalidateCanvas();
  ResizeLayout(hwnd);
}

/*------------------------------------------------------------
   Document Lifecycle Helpers
------------------------------------------------------------*/

void DocumentNew(HWND hwnd) {
  ResetToolStateForNewDocument();
  /* Layers first: HistoryInit snapshots current layers; must match new document. */
  if (!LayersInit(Canvas_GetWidth(), Canvas_GetHeight()))
    return;
  HistoryClear();
  Doc_ClearDirty();
  Doc_ClearFile();
  SyncAfterDocumentLoadOrReset(hwnd);
}

void DocumentOpen(HWND hwnd, const wchar_t *path) {
  BOOL loaded = FALSE;
  if (path) {
    loaded = LoadBitmapFromFile(path);
    if (loaded) Doc_SetFile(path);
  } else {
    loaded = FileLoad(hwnd);
  }

  if (loaded) {
    ResetToolStateForNewDocument();
    HistoryClear();
    Doc_ClearDirty();
    SyncAfterDocumentLoadOrReset(hwnd);
  }
}

BOOL DocumentConfirmDiscardOrSave(HWND hwnd) {
  if (!Doc_IsDirty())
    return TRUE;
  int r = MessageBoxW(hwnd, L"Save changes?", L"Paint", MB_YESNOCANCEL);
  if (r == IDYES)
    return FileSave(hwnd);
  if (r == IDCANCEL)
    return FALSE;
  return TRUE;
}

/*------------------------------------------------------------
   WM_COMMAND Handler
------------------------------------------------------------*/

BOOL AppCommands_OnCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  (void)lParam;

  if (HandleZoomMenu(hwnd, LOWORD(wParam)))
    return TRUE;

  switch (LOWORD(wParam)) {
  case IDM_EXIT:
    SendMessage(hwnd, WM_CLOSE, 0, 0);
    return TRUE;

  case IDM_NEW:
    if (!DocumentConfirmDiscardOrSave(hwnd))
      return TRUE;
    DocumentNew(hwnd);
    return TRUE;

  case IDM_OPEN:
    if (!DocumentConfirmDiscardOrSave(hwnd))
      return TRUE;
    DocumentOpen(hwnd, NULL);
    return TRUE;

  case IDM_SAVE:
    if (FileSave(hwnd))
      Doc_ClearDirty();
    return TRUE;

  case IDM_SAVEAS:
    if (FileSaveAs(hwnd))
      Doc_ClearDirty();
    return TRUE;

  case IDM_UNDO:
    ToolCancel();
    if (Undo()) {
        SendMessage(hwnd, WM_SIZE, 0, 0);
    }
    return TRUE;

  case IDM_REDO:
    ToolCancel();
    if (Redo()) {
        SendMessage(hwnd, WM_SIZE, 0, 0);
    }
    return TRUE;

  case IDM_COPY:
    ToolCancelSkipSelection();
    SelectionCopy();
    return TRUE;

  case IDM_CUT:
    ToolCancelSkipSelection();
    SelectionCut();
    SetDocumentDirty();
    return TRUE;

  case IDM_PASTE:
    ToolCancelSkipSelection();
    SetCurrentTool(TOOL_SELECT);
    SelectionPaste(hwnd);
    SetDocumentDirty();
    return TRUE;

  case IDM_CLEAR:
    ToolCancelSkipSelection();
    ClearSelection();
    SetDocumentDirty();
    return TRUE;

  case IDM_SELECTALL:
    ToolCancelSkipSelection();
    SetCurrentTool(TOOL_SELECT);
    SelectionSelectAll();
    return TRUE;

  case IDM_FLIPROTATE:
    ImageFlipRotate(hwnd);
    return TRUE;

  case IDM_RESIZESKEW:
    ImageResizeSkew(hwnd);
    return TRUE;

  case IDM_INVERT:
    ImageInvertColors(hwnd);
    return TRUE;

  case IDM_ATTRIBUTES:
    ImageAttributes(hwnd);
    return TRUE;

  case IDM_CLEARIMAGE:
    ImageClear(hwnd);
    return TRUE;

  case IDM_STATUSBAR:
    StatusBarSetVisible(!StatusBarIsVisible());
    ResizeLayout(hwnd);
    return TRUE;

  case IDM_TEXTTOOLBAR:
    TextToolbar_Show(!TextToolbar_IsVisible());
    return TRUE;

  case IDM_INVERT_SELECTION:
    SelectionInvert();
    return TRUE;

  case IDM_ROTATE_90:
    SelectionRotate(90);
    return TRUE;

  case IDM_ROTATE_180:
    SelectionRotate(180);
    return TRUE;

  case IDM_ROTATE_270:
    SelectionRotate(270);
    return TRUE;

  case IDM_FLIP_HORZ:
    SelectionFlip(TRUE);
    return TRUE;

  case IDM_FLIP_VERT:
    SelectionFlip(FALSE);
    return TRUE;

  case IDM_EDIT_COLORS: {
    ColorboxSyncCustomColors();
    COLORREF cr = Palette_GetPrimaryColor();
    if (ChooseColorDialog(hwnd, &cr)) {
      Palette_SetPrimaryColor(cr);
      InvalidateWindow(GetColorboxWindow());
    }
  }
    return TRUE;

  case IDM_PRESETS: {
    HWND hOptions = GetToolOptionsWindow();
    if (hOptions) {
      SetFocus(hOptions);
    }
  }
    return TRUE;
  }
  return FALSE;
}

/*------------------------------------------------------------
   WM_INITMENUPOPUP Handler
------------------------------------------------------------*/

void AppCommands_OnInitMenuPopup(HWND hwnd, WPARAM wParam, LPARAM lParam) {
  (void)lParam;

  HMENU hMenu = (HMENU)wParam;

  BOOL hasSelection = IsSelectionActive();
  UINT enableSelection = hasSelection ? MF_ENABLED : MF_GRAYED;
  UINT enablePaste = CanPasteFromClipboard() ? MF_ENABLED : MF_GRAYED;

  EnableMenuItem(hMenu, IDM_COPY, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_CUT, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_CLEAR, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_INVERT_SELECTION, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_ROTATE_90, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_ROTATE_180, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_ROTATE_270, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_FLIP_HORZ, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_FLIP_VERT, enableSelection | MF_BYCOMMAND);
  EnableMenuItem(hMenu, IDM_PASTE, enablePaste | MF_BYCOMMAND);
}
