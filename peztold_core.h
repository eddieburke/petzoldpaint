#ifndef PEZTOLD_CORE_H
#define PEZTOLD_CORE_H

#define PZTCORE_VERSION 1

/* ---- Win32 & Shell ---- */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <strsafe.h>

/* ---- Application Globals ---- */
extern HINSTANCE hInst;
extern HWND hMainWnd;

/* ---- Core Lifecycle & Global State Helpers ---- */
void SetDocumentDirty(void);

/* ---- Debug & Diagnostic Macros ---- */
#ifdef _DEBUG
#define TRACE(fmt, ...) do { \
    char buf[1024]; \
    _snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
    buf[sizeof(buf) - 1] = '\0'; \
    OutputDebugString(buf); \
} while(0)
#else
#define TRACE(fmt, ...) ((void)0)
#endif

/* ---- GDI Object Management Macros ---- */
#define SAFE_DELETE_OBJECT(h) do { if(h) { DeleteObject(h); (h) = NULL; } } while(0)
#define SAFE_DELETE_DC(h)     do { if(h) { DeleteDC(h); (h) = NULL; } } while(0)

/* ---- Error Handling Macros ---- */
#define CHECK_RETURN(expr, retval) \
    do { \
        if (!(expr)) { \
            return (retval); \
        } \
    } while(0)

#define CHECK_RETURN_VOID(expr) \
    do { \
        if (!(expr)) { \
            return; \
        } \
    } while(0)

/* ---- Application Constants ---- */
#include "constants.h"

/* ---- Shared System Headers ---- */
#include "resource.h"
#include "helpers.h"
#include "palette.h"
#include "document.h"

#endif /* PEZTOLD_CORE_H */
