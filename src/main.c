#include "peztold_core.h"
#include "app_commands.h"
#include "canvas.h"
#include "tools.h"
#include "ui/widgets/toolbar.h"
#include "tools/tool_options/tool_options.h"
#include "ui/widgets/colorbox.h"
#include "ui/widgets/statusbar.h"
#include "draw.h"
#include "ui/panels/layers_panel.h"
#include "ui/panels/history_panel.h"
#include "history.h"

#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


HINSTANCE hInst;
HWND hMainWnd;


LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void ResizeLayout(HWND hwnd);


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   PWSTR szCmdLine, int iCmdShow)
{
    static const wchar_t szAppName[] = L"PeztoldPaint";
    HWND hwnd;
    MSG msg;
    WNDCLASSW wndclass;

    (void)hPrevInstance;
    (void)szCmdLine;

    hInst = hInstance;

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    wndclass.style = CS_HREDRAW | CS_VREDRAW;
    wndclass.lpfnWndProc = WndProc;
    wndclass.cbClsExtra = 0;
    wndclass.cbWndExtra = 0;
    wndclass.hInstance = hInstance;
    wndclass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(2));
    if (!wndclass.hIcon)
        wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wndclass.lpszMenuName = MAKEINTRESOURCEW(IDM_MAINMENU);
    wndclass.lpszClassName = szAppName;

    if (!RegisterClassW(&wndclass)) {
        return 0;
    }

    hwnd = CreateWindowW(szAppName, L"Peztold Paint",
                       WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       CW_USEDEFAULT, CW_USEDEFAULT,
                       NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        return 0;
    }

    hMainWnd = hwnd;
    ShowWindow(hwnd, iCmdShow);
    UpdateWindow(hwnd);

    HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(2));
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    DestroyCanvas();
    return (int)msg.wParam;
}


void ResizeLayout(HWND hwnd)
{
    RECT rc;
    int nStatusHeight;
    int nBottomOffset;

    GetClientRect(hwnd, &rc);
    nStatusHeight = StatusBarGetHeight();

    HWND hStatus = GetStatusBarWindow();
    if (hStatus)
        MoveWindow(hStatus, 0, rc.bottom - nStatusHeight,
                  rc.right, nStatusHeight, TRUE);

    nBottomOffset = nStatusHeight + COLORBOX_HEIGHT;
    int nToolbarButtonsHeight = GetToolbarHeight();

    HWND hToolbar = GetToolbarWindow();
    if (hToolbar)
        MoveWindow(hToolbar, 0, 0, TOOLBAR_WIDTH,
                  nToolbarButtonsHeight, TRUE);

    HWND hToolOptions = GetToolOptionsWindow();
    if (hToolOptions)
        MoveWindow(hToolOptions, 0, nToolbarButtonsHeight, TOOLBAR_WIDTH,
                  rc.bottom - nBottomOffset - nToolbarButtonsHeight, TRUE);

    HWND hColorbox = GetColorboxWindow();
    if (hColorbox)
        MoveWindow(hColorbox, 0, rc.bottom - nBottomOffset,
                  rc.right, COLORBOX_HEIGHT, TRUE);

    HWND hCanvas = GetCanvasWindow();
    if (hCanvas) {
        int nCanvasX = TOOLBAR_WIDTH + 2;
        int nCanvasY = 2;
        int nCanvasW = rc.right - nCanvasX - 2;
        int nCanvasH = rc.bottom - nBottomOffset - nCanvasY - 2;
        MoveWindow(hCanvas, nCanvasX, nCanvasY, nCanvasW, nCanvasH, TRUE);
    }
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        if (!CreateCanvas(800, 600)) {
            return -1;
        }
        InitializeTools();
        HistoryInit();
        CreateToolbar(hwnd);
        CreateToolOptions(hwnd);
        CreateLayersPanel(hwnd);
        CreateHistoryPanel(hwnd);
        CreateColorbox(hwnd);
        CreateStatusBar(hwnd);
        CreateCanvasWindow(hwnd);
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_SIZE:
        ResizeLayout(hwnd);
        return 0;

    case WM_CONTEXTMENU:
        {
            int screenX = (int)(short)LOWORD(lParam);
            int screenY = (int)(short)HIWORD(lParam);

            if (screenX == -1) {
                POINT pt;
                GetCursorPos(&pt);
                screenX = pt.x;
                screenY = pt.y;
            }

            int curTool = Tool_GetCurrent();
            if (curTool == TOOL_SELECT || curTool == TOOL_FREEFORM ||
                curTool == TOOL_TEXT) {
                HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CONTEXT_MENU));
                HMENU hSub = GetSubMenu(hMenu, 0);

                TrackPopupMenu(hSub, TPM_RIGHTBUTTON, screenX, screenY,
                             0, hwnd, NULL);
                DestroyMenu(hMenu);
                return 0;
            }
        }
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_COMMAND:
        if (AppCommands_OnCommand(hwnd, wParam, lParam))
            return 0;
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        ClearClientRect(hdc, hwnd, GetSysColorBrush(COLOR_APPWORKSPACE));
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_DROPFILES:
        {
            HDROP hDrop = (HDROP)wParam;
            wchar_t szPath[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, szPath, MAX_PATH)) {
                if (DocumentConfirmDiscardOrSave(hwnd))
                    DocumentOpen(hwnd, szPath);
            }
            DragFinish(hDrop);
        }
        return 0;

    case WM_INITMENUPOPUP:
        AppCommands_OnInitMenuPopup(hwnd, wParam, lParam);
        return 0;

    case WM_CLOSE:
        if (!DocumentConfirmDiscardOrSave(hwnd))
            return 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
