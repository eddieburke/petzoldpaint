/*------------------------------------------------------------
   FILE_IO.C -- File Input/Output Operations
   
   This module handles loading and saving bitmap files in
   both BMP and PNG formats using Windows Imaging Component.
  ------------------------------------------------------------*/

#include "peztold_core.h"
#include "canvas.h"
#include "file_io.h"
#include "layers.h"
#include <commdlg.h>
#include <objbase.h>
#include <wincodec.h>


/*------------------------------------------------------------
   Static Variables
  ------------------------------------------------------------*/

static IWICImagingFactory* g_wicFactory = NULL;
static BOOL g_comInitialized = FALSE;
static BOOL g_comInitializedByUs = FALSE;

static float g_jpegQuality = 0.9f;
static BYTE g_jpegSubsampling = 0x01; // 4:2:0
static BOOL g_jpegFlattenBg = TRUE;

/*------------------------------------------------------------
   WIC Factory Management
  ------------------------------------------------------------*/

static BOOL EnsureWicFactory(void)
{
    if (!g_comInitialized) {
        HRESULT hrInit = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        if (FAILED(hrInit) && hrInit != RPC_E_CHANGED_MODE) {
            return FALSE;
        }
        g_comInitialized = TRUE;
        g_comInitializedByUs = (hrInit == S_OK || hrInit == S_FALSE);
    }

    if (!g_wicFactory) {
        HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                                     CLSCTX_INPROC_SERVER,
                                     &IID_IWICImagingFactory,
                                     (LPVOID*)&g_wicFactory);
        if (FAILED(hr))
            return FALSE;
    }
    return TRUE;
}

IWICImagingFactory* FileIO_GetWicFactory(void)
{
    if (EnsureWicFactory())
        return g_wicFactory;
    return NULL;
}

void FileIO_ShutdownCom(void)
{
    if (g_wicFactory) {
        g_wicFactory->lpVtbl->Release(g_wicFactory);
        g_wicFactory = NULL;
    }
    if (g_comInitializedByUs) {
        CoUninitialize();
    }
    g_comInitialized = FALSE;
    g_comInitializedByUs = FALSE;
}

/*------------------------------------------------------------
   File Extension Helpers
  ------------------------------------------------------------*/

static BOOL HasExtension(const char* path, const char* ext)
{
    const char* dot = strrchr(path, '.');
    if (!dot)
        return FALSE;
    return _stricmp(dot, ext) == 0;
}

static void EnsureExtensionForSave(char* path, size_t maxLen, const char* ext)
{
    const char* dot = strrchr(path, '.');
    const char* slash1 = strrchr(path, '\\');
    const char* slash2 = strrchr(path, '/');
    const char* lastSlash = slash1 > slash2 ? slash1 : slash2;

    if (!dot || (lastSlash && dot < lastSlash)) {
        size_t len = strlen(path);
        size_t extLen = strlen(ext);
        if (len + extLen + 1 < maxLen) {
            strcat(path, ext);
        }
    }
}

/*------------------------------------------------------------
    WIC Error Reporting Helper
------------------------------------------------------------*/

static void ShowWicError(HRESULT hr, const char* operation)
{
    char msg[512];
    char hrMsg[256] = "";
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, hr, 0, hrMsg, sizeof(hrMsg), NULL);
    StringCchPrintf(msg, sizeof(msg), "%s failed.\nHRESULT: 0x%08lX\n%s",
                    operation, hr, hrMsg);
    MessageBox(hMainWnd, msg, "Image Error", MB_ICONERROR);
}

/*------------------------------------------------------------
    PNG Loading Function
------------------------------------------------------------*/

static BOOL LoadWicImage(const char* szPath)
{
    if (!EnsureWicFactory())
        return FALSE;

    WCHAR wPath[MAX_PATH];
    if (MultiByteToWideChar(CP_ACP, 0, szPath, -1, wPath, MAX_PATH) <= 0) {
        return FALSE;
    }

    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    BOOL ok = FALSE;

    HRESULT hr = g_wicFactory->lpVtbl->CreateDecoderFromFilename(
        g_wicFactory, wPath, NULL, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);

    if (SUCCEEDED(hr)) {
        hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    }
    if (SUCCEEDED(hr)) {
        hr = g_wicFactory->lpVtbl->CreateFormatConverter(g_wicFactory, &converter);
    }
    if (SUCCEEDED(hr)) {
        hr = converter->lpVtbl->Initialize(
            converter, (IWICBitmapSource*)frame, &GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }

    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) {
        hr = frame->lpVtbl->GetSize(frame, &width, &height);
    }

    if (SUCCEEDED(hr) && width > 0 && height > 0) {
        int stride = (int)width * 4;
        BYTE* pixels = (BYTE*)malloc(stride * height);
        if (pixels) {
            hr = converter->lpVtbl->CopyPixels(converter, NULL, stride,
                                              stride * height, pixels);
            if (SUCCEEDED(hr)) {
                ok = LayersLoadFromPixels((int)width, (int)height, pixels,
                                         stride, TRUE);
            }
            free(pixels);
        }
    }

    if (converter)
        converter->lpVtbl->Release(converter);
    if (frame)
        frame->lpVtbl->Release(frame);
    if (decoder)
        decoder->lpVtbl->Release(decoder);

    if (!ok) ShowWicError(hr, "Load image");
    return ok;
}

/*------------------------------------------------------------
    JPEG Options Dialog
  ------------------------------------------------------------*/

INT_PTR CALLBACK JpegOptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemInt(hDlg, IDC_JPEG_QUALITY, (UINT)(g_jpegQuality * 100), FALSE);
        SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_ADDSTRING, 0, (LPARAM)"4:2:0 (Standard)");
        SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_ADDSTRING, 0, (LPARAM)"4:2:2");
        SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_ADDSTRING, 0, (LPARAM)"4:4:4 (High Quality)");
        SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_ADDSTRING, 0, (LPARAM)"4:4:0");

        SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_SETCURSEL, g_jpegSubsampling - 1, 0);
        CheckDlgButton(hDlg, IDC_JPEG_USE_BG_COLOR, g_jpegFlattenBg ? BST_CHECKED : BST_UNCHECKED);
        return (INT_PTR)TRUE;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            int q = GetDlgItemInt(hDlg, IDC_JPEG_QUALITY, NULL, FALSE);
            if (q < 0) q = 0; if (q > 100) q = 100;
            g_jpegQuality = (float)q / 100.0f;

            LRESULT sel = SendDlgItemMessage(hDlg, IDC_JPEG_SUBSAMPLING, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                g_jpegSubsampling = (BYTE)(sel + 1);
            }
            g_jpegFlattenBg = IsDlgButtonChecked(hDlg, IDC_JPEG_USE_BG_COLOR) == BST_CHECKED;
            EndDialog(hDlg, IDOK);
            return (INT_PTR)TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, IDCANCEL);
            return (INT_PTR)TRUE;
        }
        break;
    }
    }
    return (INT_PTR)FALSE;
}

/*------------------------------------------------------------
   WIC Saving Function
  ------------------------------------------------------------*/

static BOOL SaveWicImage(const char* lpszFileName, REFGUID containerFormat)
{
    if (!EnsureWicFactory())
        return FALSE;

    BYTE* bits = NULL;
    HBITMAP hBmp = NULL;
    
    if (IsEqualGUID(containerFormat, &GUID_ContainerFormatJpeg) && g_jpegFlattenBg) {
        hBmp = LayersFlattenToBitmap(Palette_GetSecondaryColor());
        if (hBmp) {
            BITMAP bm;
            GetObject(hBmp, sizeof(bm), &bm);
            bits = (BYTE*)bm.bmBits;
        }
    } else {
        hBmp = LayersFlattenToBitmapWithAlpha(&bits);
    }

    if (!hBmp || !bits) {
        if (hBmp)
            DeleteObject(hBmp);
        return FALSE;
    }

    WCHAR wPath[MAX_PATH];
    BOOL ok = FALSE;

    IWICStream* stream = NULL;
    IWICBitmapEncoder* encoder = NULL;
    IWICBitmapFrameEncode* frame = NULL;
    IPropertyBag2* props = NULL;

    if (MultiByteToWideChar(CP_ACP, 0, lpszFileName, -1, wPath, MAX_PATH) > 0) {
        HRESULT hr = g_wicFactory->lpVtbl->CreateStream(g_wicFactory, &stream);
        if (SUCCEEDED(hr)) {
            hr = stream->lpVtbl->InitializeFromFilename(stream, wPath,
                                                        GENERIC_WRITE);
        }
        if (SUCCEEDED(hr)) {
            hr = g_wicFactory->lpVtbl->CreateEncoder(g_wicFactory,
                                                     containerFormat,
                                                     NULL, &encoder);
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->lpVtbl->Initialize(encoder, (IStream*)stream,
                                             WICBitmapEncoderNoCache);
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->lpVtbl->CreateNewFrame(encoder, &frame, &props);
        }
        if (SUCCEEDED(hr) && props && IsEqualGUID(containerFormat, &GUID_ContainerFormatJpeg)) {
            PROPBAG2 prop = { 0 };
            VARIANT var;

            // Quality
            prop.pstrName = L"ImageQuality";
            VariantInit(&var);
            var.vt = VT_R4;
            var.fltVal = g_jpegQuality;
            props->lpVtbl->Write(props, 1, &prop, &var);

            // Subsampling
            prop.pstrName = L"JpegYCrCbSubsampling";
            VariantInit(&var);
            var.vt = VT_UI1;
            var.bVal = g_jpegSubsampling;
            props->lpVtbl->Write(props, 1, &prop, &var);
        }
        if (SUCCEEDED(hr)) {
            hr = frame->lpVtbl->Initialize(frame, props);
        }
        if (SUCCEEDED(hr)) {
            hr = frame->lpVtbl->SetSize(frame, Canvas_GetWidth(), Canvas_GetHeight());
        }

        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(hr)) {
            hr = frame->lpVtbl->SetPixelFormat(frame, &format);
        }

        if (SUCCEEDED(hr)) {
            IWICBitmap* wicBitmap = NULL;
            hr = g_wicFactory->lpVtbl->CreateBitmapFromMemory(g_wicFactory,
                Canvas_GetWidth(), Canvas_GetHeight(),
                &GUID_WICPixelFormat32bppBGRA,
                Canvas_GetWidth() * 4,
                Canvas_GetWidth() * Canvas_GetHeight() * 4,
                bits, &wicBitmap);

            if (SUCCEEDED(hr)) {
                IWICFormatConverter* converter = NULL;
                hr = g_wicFactory->lpVtbl->CreateFormatConverter(g_wicFactory, &converter);
                if (SUCCEEDED(hr)) {
                    hr = converter->lpVtbl->Initialize(converter, (IWICBitmapSource*)wicBitmap,
                        &format, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

                    if (SUCCEEDED(hr)) {
                        hr = frame->lpVtbl->WriteSource(frame, (IWICBitmapSource*)converter, NULL);
                    }
                    converter->lpVtbl->Release(converter);
                }
                wicBitmap->lpVtbl->Release(wicBitmap);
            }
        }

        if (SUCCEEDED(hr)) {
            hr = frame->lpVtbl->Commit(frame);
        }
        if (SUCCEEDED(hr)) {
            hr = encoder->lpVtbl->Commit(encoder);
        }
        ok = SUCCEEDED(hr);
    }

    if (props)
        props->lpVtbl->Release(props);
    if (frame)
        frame->lpVtbl->Release(frame);
    if (encoder)
        encoder->lpVtbl->Release(encoder);
    if (stream)
        stream->lpVtbl->Release(stream);

    DeleteObject(hBmp);
    return ok;
}

/*------------------------------------------------------------
   FileLoad
   
   Displays the file open dialog and loads the selected file.
  ------------------------------------------------------------*/

BOOL FileLoad(HWND hWnd)
{
    OPENFILENAME ofn;
    char szFile[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Image Files\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff\0"
                     "Bitmap Files (*.bmp)\0*.bmp\0"
                     "PNG Files (*.png)\0*.png\0"
                     "JPEG Files (*.jpg, *.jpeg)\0*.jpg;*.jpeg\0"
                     "GIF Files (*.gif)\0*.gif\0"
                     "TIFF Files (*.tif, *.tiff)\0*.tif;*.tiff\0"
                     "All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        if (LoadBitmapFromFile(ofn.lpstrFile)) {
            InvalidateRect(hWnd, NULL, TRUE);
            Doc_SetFile(szFile);
            return TRUE;
        }
    }
    return FALSE;
}

/*------------------------------------------------------------
   LoadBitmapFromFile
   
   Loads a bitmap file (BMP or PNG) into the layers system.
  ------------------------------------------------------------*/

BOOL LoadBitmapFromFile(const char* szPath)
{
    if (HasExtension(szPath, ".png")  || 
        HasExtension(szPath, ".jpg")  || 
        HasExtension(szPath, ".jpeg") ||
        HasExtension(szPath, ".gif")  ||
        HasExtension(szPath, ".tif")  ||
        HasExtension(szPath, ".tiff")) {
        return LoadWicImage(szPath);
    }

    HBITMAP hNewBmp = (HBITMAP)LoadImage(NULL, szPath, IMAGE_BITMAP, 0, 0,
                                         LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hNewBmp)
        return FALSE;

    BOOL ok = LayersLoadFromBitmap(hNewBmp);
    DeleteObject(hNewBmp);
    return ok;
}

/*------------------------------------------------------------
   SaveBitmapToFile
   
   Saves a bitmap to a BMP file using GDI functions.
  ------------------------------------------------------------*/

BOOL SaveBitmapToFile(HBITMAP hBmp, LPCSTR lpszFileName)
{
    if (HasExtension(lpszFileName, ".png")) {
        return SaveWicImage(lpszFileName, &GUID_ContainerFormatPng);
    }
    if (HasExtension(lpszFileName, ".jpg") || HasExtension(lpszFileName, ".jpeg")) {
        return SaveWicImage(lpszFileName, &GUID_ContainerFormatJpeg);
    }

    HDC hDC = GetScreenDC();
    if (!hDC)
        return FALSE;

    BITMAP bmp;
    if (!GetObject(hBmp, sizeof(BITMAP), &bmp)) {
        ReleaseScreenDC(hDC);
        return FALSE;
    }

    WORD cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);
    if (cClrBits <= 1)
        cClrBits = 1;
    else if (cClrBits <= 4)
        cClrBits = 4;
    else if (cClrBits <= 8)
        cClrBits = 8;
    else if (cClrBits <= 16)
        cClrBits = 16;
    else if (cClrBits <= 24)
        cClrBits = 24;
    else
        cClrBits = 32;

    DWORD colorCount = (cClrBits <= 8) ? (1U << cClrBits) : 0;
    PBITMAPINFO pbmi = (PBITMAPINFO)LocalAlloc(LPTR,
        sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * colorCount);
    if (!pbmi) {
        ReleaseScreenDC(hDC);
        return FALSE;
    }

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pbmi->bmiHeader.biWidth = bmp.bmWidth;
    pbmi->bmiHeader.biHeight = bmp.bmHeight;
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;
    pbmi->bmiHeader.biClrUsed = colorCount;
    pbmi->bmiHeader.biCompression = BI_RGB;
    pbmi->bmiHeader.biSizeImage = ((bmp.bmWidth * cClrBits + 31) & ~31) / 8 *
                                  bmp.bmHeight;
    pbmi->bmiHeader.biClrImportant = 0;

    DWORD imageSize = pbmi->bmiHeader.biSizeImage;
    LPBYTE lpBits = (LPBYTE)GlobalAlloc(GMEM_FIXED, imageSize);
    if (!lpBits) {
        LocalFree(pbmi);
        ReleaseScreenDC(hDC);
        return FALSE;
    }

    if (!GetDIBits(hDC, hBmp, 0, pbmi->bmiHeader.biHeight, lpBits, pbmi,
                  DIB_RGB_COLORS)) {
        GlobalFree(lpBits);
        LocalFree(pbmi);
        ReleaseScreenDC(hDC);
        return FALSE;
    }

    HANDLE hf = CreateFile(lpszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL,
                          CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) {
        GlobalFree(lpBits);
        LocalFree(pbmi);
        ReleaseScreenDC(hDC);
        return FALSE;
    }

    BITMAPFILEHEADER hdr;
    hdr.bfType = 0x4d42;
    hdr.bfSize = sizeof(BITMAPFILEHEADER) + pbmi->bmiHeader.biSize +
                 colorCount * sizeof(RGBQUAD) + imageSize;
    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;
    hdr.bfOffBits = sizeof(BITMAPFILEHEADER) + pbmi->bmiHeader.biSize +
                    colorCount * sizeof(RGBQUAD);

    DWORD dwTmp;
    BOOL bSuccess = TRUE;

    bSuccess = bSuccess && WriteFile(hf, &hdr, sizeof(hdr), &dwTmp, NULL);
    bSuccess = bSuccess && WriteFile(hf, pbmi,
                                     sizeof(BITMAPINFOHEADER) +
                                     colorCount * sizeof(RGBQUAD),
                                     &dwTmp, NULL);
    bSuccess = bSuccess && WriteFile(hf, lpBits, imageSize, &dwTmp, NULL);

    CloseHandle(hf);
    GlobalFree(lpBits);
    LocalFree(pbmi);
    ReleaseScreenDC(hDC);

    return bSuccess;
}

/*------------------------------------------------------------
   FileSave
   
   Saves the current document to the previously opened file.
  ------------------------------------------------------------*/

BOOL FileSave(HWND hWnd)
{
    const char *file = Doc_GetFile();
    if (strlen(file) > 0) {
        if (HasExtension(file, ".png")) {
            return SaveWicImage(file, &GUID_ContainerFormatPng);
        }
        if (HasExtension(file, ".jpg") || HasExtension(file, ".jpeg")) {
            return SaveWicImage(file, &GUID_ContainerFormatJpeg);
        }

        HBITMAP hFlat = LayersFlattenToBitmap(Palette_GetSecondaryColor());
        if (!hFlat)
            return FALSE;
        BOOL ok = SaveBitmapToFile(hFlat, file);
        DeleteObject(hFlat);
        return ok;
    }
    return FileSaveAs(hWnd);
}

/*------------------------------------------------------------
   FileSaveAs
   
   Displays the file save dialog and saves the document.
  ------------------------------------------------------------*/

BOOL FileSaveAs(HWND hWnd)
{
    OPENFILENAME ofn;
    char szFile[MAX_PATH] = "";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "PNG Files (*.png)\0*.png\0"
                     "Bitmap Files (*.bmp)\0*.bmp\0"
                     "JPEG Files (*.jpg)\0*.jpg\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        if (!HasExtension(ofn.lpstrFile, ".png") &&
            !HasExtension(ofn.lpstrFile, ".bmp") &&
            !HasExtension(ofn.lpstrFile, ".jpg") &&
            !HasExtension(ofn.lpstrFile, ".jpeg")) {
            if (ofn.nFilterIndex == 1) {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, ".png");
            } else if (ofn.nFilterIndex == 3) {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, ".jpg");
            } else {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, ".bmp");
            }
        }

        if (HasExtension(ofn.lpstrFile, ".png")) {
            if (SaveWicImage(ofn.lpstrFile, &GUID_ContainerFormatPng)) {
                Doc_SetFile(ofn.lpstrFile);
                return TRUE;
            }
            return FALSE;
        }

        if (HasExtension(ofn.lpstrFile, ".jpg") || HasExtension(ofn.lpstrFile, ".jpeg")) {
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_JPEG_OPTIONS), hWnd, JpegOptionsDlgProc) == IDCANCEL) {
                return FALSE;
            }
            if (SaveWicImage(ofn.lpstrFile, &GUID_ContainerFormatJpeg)) {
                Doc_SetFile(ofn.lpstrFile);
                return TRUE;
            }
            return FALSE;
        }

        HBITMAP hFlat = LayersFlattenToBitmap(Palette_GetSecondaryColor());
        if (hFlat && SaveBitmapToFile(hFlat, ofn.lpstrFile)) {
            DeleteObject(hFlat);
            Doc_SetFile(ofn.lpstrFile);
            return TRUE;
        }
        if (hFlat)
            DeleteObject(hFlat);
    }
    return FALSE;
}
