#ifndef PEZTOLD_CORE_H
#define PEZTOLD_CORE_H

#define PZTCORE_VERSION 1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <strsafe.h>
extern HINSTANCE hInst;
extern HWND hMainWnd;

typedef enum {
  EV_PIXELS_CHANGED,
  EV_LAYER_CONFIG,
  EV_DOC_RESET
} CoreEvent;

void Core_RegisterObserver(void (*fn)(CoreEvent ev));
void Core_Notify(CoreEvent ev);
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
#define SAFE_DELETE_OBJECT(h) do { if(h) { DeleteObject(h); (h) = NULL; } } while(0)
#define SAFE_DELETE_DC(h)     do { if(h) { DeleteDC(h); (h) = NULL; } } while(0)
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
#include "constants.h"
#include "resource.h"
#include "helpers.h"
#include "palette.h"
#include "document.h"

#endif
