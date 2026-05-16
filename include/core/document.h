#ifndef DOCUMENT_H
#define DOCUMENT_H
#include <windows.h>
typedef struct {
	int width;
	int height;
	BOOL dirty;
	wchar_t currentFile[MAX_PATH];
	int scrollX;
	int scrollY;
	double zoomPercent;
} Document;
Document *GetDocument(void);
int Doc_GetWidth(void);
int Doc_GetHeight(void);
void Doc_SetSize(int w, int h);
BOOL Doc_IsDirty(void);
void Doc_ClearDirty(void);
void SetDocumentDirty(void);
const wchar_t *Doc_GetFile(void);
void Doc_SetFile(const wchar_t *path);
void Doc_ClearFile(void);
int Doc_GetScrollX(void);
int Doc_GetScrollY(void);
double Doc_GetZoom(void);
void Doc_SetZoom(double z);
#endif
