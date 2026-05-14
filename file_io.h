#ifndef FILE_IO_H
#define FILE_IO_H
#include <windows.h>
#include <wincodec.h>
BOOL FileLoad(HWND hWnd);
BOOL FileSave(HWND hWnd);
BOOL FileSaveAs(HWND hWnd);
BOOL LoadBitmapFromFile(const char* szPath);
IWICImagingFactory* FileIO_GetWicFactory(void);
void FileIO_ShutdownCom(void);
#endif
