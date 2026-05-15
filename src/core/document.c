#include "document.h"
#include <string.h>
#include <strsafe.h>


static Document g_doc = { 800, 600, FALSE, L"", 0, 0, 100.0 };

Document* GetDocument(void) { return &g_doc; }

int Doc_GetWidth(void)          { return g_doc.width; }
int Doc_GetHeight(void)         { return g_doc.height; }
void Doc_SetSize(int w, int h)  { g_doc.width = w; g_doc.height = h; }

BOOL Doc_IsDirty(void)          { return g_doc.dirty; }
void Doc_SetDirty(void)         { g_doc.dirty = TRUE; }
void Doc_ClearDirty(void)       { g_doc.dirty = FALSE; }
void SetDocumentDirty(void)     { Doc_SetDirty(); }

const wchar_t* Doc_GetFile(void) { return g_doc.currentFile; }
void Doc_SetFile(const wchar_t *p) {
    if (p) {
        StringCchCopyW(g_doc.currentFile, MAX_PATH, p);
    } else {
        g_doc.currentFile[0] = L'\0';
    }
}
void Doc_ClearFile(void)        { g_doc.currentFile[0] = L'\0'; }

int Doc_GetScrollX(void)        { return g_doc.scrollX; }
int Doc_GetScrollY(void)        { return g_doc.scrollY; }

double Doc_GetZoom(void)        { return g_doc.zoomPercent; }
void Doc_SetZoom(double z)      { g_doc.zoomPercent = z; }
