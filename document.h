#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <windows.h>

/*------------------------------------------------------------
   Document — central application state container

   Consolidates canvas dimensions, dirty flag, current file,
   scroll position, and zoom into a single struct.
   Replaces the formerly scattered globals (bDocumentDirty,
   szCurrentFile, canvas static vars).
------------------------------------------------------------*/

typedef struct {
    int width;              /* Canvas width  (pixels)        */
    int height;             /* Canvas height (pixels)        */
    BOOL dirty;             /* Unsaved changes flag          */
    wchar_t currentFile[MAX_PATH]; /* Current file path (or L"") */
    int scrollX;            /* Horizontal scroll offset      */
    int scrollY;            /* Vertical scroll offset        */
    double zoomPercent;     /* Zoom level (12.5 – 800.0)    */
} Document;

/* Return the singleton document instance */
Document* GetDocument(void);

/* Convenience accessors */
int         Doc_GetWidth(void);
int         Doc_GetHeight(void);
void        Doc_SetSize(int w, int h);
BOOL        Doc_IsDirty(void);
void        Doc_SetDirty(void);
void        Doc_ClearDirty(void);
const wchar_t* Doc_GetFile(void);
void           Doc_SetFile(const wchar_t *path);
void        Doc_ClearFile(void);
int         Doc_GetScrollX(void);
int         Doc_GetScrollY(void);
double      Doc_GetZoom(void);
void        Doc_SetZoom(double z);

#endif /* DOCUMENT_H */
