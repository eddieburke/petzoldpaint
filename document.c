#include "document.h"
#include <string.h>
#include <strsafe.h>

/*------------------------------------------------------------
   Singleton document instance
------------------------------------------------------------*/

static Document g_doc = {
    800,                    /* width         */
    600,                    /* height        */
    FALSE,                  /* dirty         */
    "",                     /* currentFile   */
    0,                      /* scrollX       */
    0,                      /* scrollY       */
    100.0                   /* zoomPercent   */
};

/*------------------------------------------------------------
   Accessors
------------------------------------------------------------*/

Document* GetDocument(void) {
    return &g_doc;
}

/* Dimensions */
int Doc_GetWidth(void)          { return g_doc.width; }
int Doc_GetHeight(void)         { return g_doc.height; }
void Doc_SetSize(int w, int h)  { g_doc.width = w; g_doc.height = h; }

/* Dirty flag */
BOOL Doc_IsDirty(void)          { return g_doc.dirty; }
void Doc_SetDirty(void)         { g_doc.dirty = TRUE; }
void Doc_ClearDirty(void)       { g_doc.dirty = FALSE; }

/* Current file */
const char* Doc_GetFile(void)   { return g_doc.currentFile; }
void Doc_SetFile(const char *p) {
    if (p) {
        StringCchCopy(g_doc.currentFile, MAX_PATH, p);
    } else {
        g_doc.currentFile[0] = '\0';
    }
}
void Doc_ClearFile(void)        { g_doc.currentFile[0] = '\0'; }

/* Scroll */
int Doc_GetScrollX(void)        { return g_doc.scrollX; }
int Doc_GetScrollY(void)        { return g_doc.scrollY; }
void Doc_SetScroll(int x, int y){ g_doc.scrollX = x; g_doc.scrollY = y; }

/* Zoom */
double Doc_GetZoom(void)        { return g_doc.zoomPercent; }
void Doc_SetZoom(double z)      { g_doc.zoomPercent = z; }
