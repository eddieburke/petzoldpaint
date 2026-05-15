#include "peztold_core.h"
#include "canvas.h"
#include "file_io.h"
#include "gdi_utils.h"
#include "layers.h"
#include <commdlg.h>
#include <limits.h>
#include <objbase.h>
#include <stdint.h>
#include <wincodec.h>



static IWICImagingFactory* g_wicFactory = NULL;
static BOOL g_comInitialized = FALSE;
static BOOL g_comInitializedByUs = FALSE;

static float g_jpegQuality = 0.9f;
static BYTE g_jpegSubsampling = 0x01; // 4:2:0
static BOOL g_jpegFlattenBg = TRUE;

#define MAX_CANVAS_DIM 16384U


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


static BOOL HasExtension(const wchar_t* path, const wchar_t* ext)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    if (!dot)
        return FALSE;
    return _wcsicmp(dot, ext) == 0;
}

static void EnsureExtensionForSave(wchar_t* path, size_t maxLen, const wchar_t* ext)
{
    const wchar_t* dot = wcsrchr(path, L'.');
    const wchar_t* slash1 = wcsrchr(path, L'\\');
    const wchar_t* slash2 = wcsrchr(path, L'/');
    const wchar_t* lastSlash = slash1 > slash2 ? slash1 : slash2;

    if (!dot || (lastSlash && dot < lastSlash)) {
        size_t len = wcslen(path);
        size_t extLen = wcslen(ext);
        if (len + extLen + 1 < maxLen) {
            StringCchCatW(path, maxLen, ext);
        }
    }
}


static void ShowWicError(HRESULT hr, const char* operation)
{
    char msg[512];
    char hrMsg[256] = "";
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, hr, 0, hrMsg, sizeof(hrMsg), NULL);
    StringCchPrintfA(msg, sizeof(msg), "%s failed.\nHRESULT: 0x%08lX\n%s",
                    operation, hr, hrMsg);
    MessageBoxA(hMainWnd, msg, "Image Error", MB_ICONERROR);
}


static BOOL LoadWicPixels(IWICFormatConverter* converter, UINT width, UINT height) {
    if (width == 0 || height == 0 || width > MAX_CANVAS_DIM || height > MAX_CANVAS_DIM) return FALSE;
    
    size_t rowBytes = (size_t)width * 4u;
    if (rowBytes > SIZE_MAX / (size_t)height) return FALSE;
    size_t totalBytes = rowBytes * (size_t)height;

    if (width > (UINT)INT_MAX || height > (UINT)INT_MAX || rowBytes > (size_t)INT_MAX || totalBytes > (size_t)UINT_MAX) return FALSE;

    BYTE* pixels = (BYTE*)malloc(totalBytes);
    if (!pixels) return FALSE;

    HRESULT hr = converter->lpVtbl->CopyPixels(converter, NULL, (UINT)rowBytes, (UINT)totalBytes, pixels);
    BOOL ok = FALSE;
    if (SUCCEEDED(hr)) {
        ok = LayersLoadFromPixels((int)width, (int)height, pixels, (int)rowBytes, TRUE);
    }
    free(pixels);
    return ok;
}

static BOOL LoadWicImage(const wchar_t* szPath)
{
    if (!EnsureWicFactory()) return FALSE;

    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    HRESULT hr = g_wicFactory->lpVtbl->CreateDecoderFromFilename(g_wicFactory, szPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);

    if (SUCCEEDED(hr)) hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
    if (SUCCEEDED(hr)) hr = g_wicFactory->lpVtbl->CreateFormatConverter(g_wicFactory, &converter);
    if (SUCCEEDED(hr)) {
        hr = converter->lpVtbl->Initialize(converter, (IWICBitmapSource*)frame, &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }

    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) hr = frame->lpVtbl->GetSize(frame, &width, &height);

    BOOL ok = FALSE;
    if (SUCCEEDED(hr)) {
        ok = LoadWicPixels(converter, width, height);
        if (!ok && hr == S_OK) hr = E_FAIL;
    }

    if (converter) converter->lpVtbl->Release(converter);
    if (frame) frame->lpVtbl->Release(frame);
    if (decoder) decoder->lpVtbl->Release(decoder);

    if (!ok) ShowWicError(hr, "Load image");
    return ok;
}


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


static BOOL SaveWicImage(const wchar_t* lpszFileName, REFGUID containerFormat)
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

    BOOL ok = FALSE;

    IWICStream* stream = NULL;
    IWICBitmapEncoder* encoder = NULL;
    IWICBitmapFrameEncode* frame = NULL;
    IPropertyBag2* props = NULL;

    HRESULT hr = S_OK;
    if (lpszFileName && lpszFileName[0] != L'\0') {
        hr = g_wicFactory->lpVtbl->CreateStream(g_wicFactory, &stream);
        if (SUCCEEDED(hr)) {
            hr = stream->lpVtbl->InitializeFromFilename(stream, lpszFileName,
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
            hr = props->lpVtbl->Write(props, 1, &prop, &var);
            if (FAILED(hr)) goto cleanup;

            // Subsampling
            prop.pstrName = L"JpegYCrCbSubsampling";
            VariantInit(&var);
            var.vt = VT_UI1;
            var.bVal = g_jpegSubsampling;
            hr = props->lpVtbl->Write(props, 1, &prop, &var);
            if (FAILED(hr)) goto cleanup;
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

cleanup:
    if (props) props->lpVtbl->Release(props);
    if (frame) frame->lpVtbl->Release(frame);
    if (encoder) encoder->lpVtbl->Release(encoder);
    if (stream) stream->lpVtbl->Release(stream);

    if (!ok) ShowWicError(hr, "Save image");
    DeleteObject(hBmp);
    return ok;
}


BOOL FileLoad(HWND hWnd)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All Image Files\0*.bmp;*.png;*.jpg;*.jpeg;*.gif;*.tif;*.tiff\0"
                      L"Bitmap Files (*.bmp)\0*.bmp\0"
                      L"PNG Files (*.png)\0*.png\0"
                      L"JPEG Files (*.jpg, *.jpeg)\0*.jpg;*.jpeg\0"
                      L"GIF Files (*.gif)\0*.gif\0"
                      L"TIFF Files (*.tif, *.tiff)\0*.tif;*.tiff\0"
                      L"All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        if (LoadBitmapFromFile(ofn.lpstrFile)) {
            InvalidateRect(hWnd, NULL, TRUE);
            Doc_SetFile(szFile);
            return TRUE;
        }
    }

    DWORD dlgErr = CommDlgExtendedError();
    if (dlgErr != 0) {
        wchar_t msg[128];
        StringCchPrintfW(msg, 128, L"Open dialog failed (0x%08lX).", dlgErr);
        MessageBoxW(hWnd, msg, L"File Open Error", MB_ICONERROR);
    }

    return FALSE;
}


BOOL LoadBitmapFromFile(const wchar_t* szPath)
{
    if (HasExtension(szPath, L".png")  ||
        HasExtension(szPath, L".jpg")  ||
        HasExtension(szPath, L".jpeg") ||
        HasExtension(szPath, L".gif")  ||
        HasExtension(szPath, L".tif")  ||
        HasExtension(szPath, L".tiff")) {
        return LoadWicImage(szPath);
    }

    HBITMAP hNewBmp = (HBITMAP)LoadImageW(NULL, szPath, IMAGE_BITMAP, 0, 0,
                                         LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (!hNewBmp)
        return FALSE;

    BOOL ok = LayersLoadFromBitmap(hNewBmp);
    DeleteObject(hNewBmp);
    return ok;
}




BOOL FileSave(HWND hWnd)
{
    const wchar_t *file = Doc_GetFile();
    if (wcslen(file) > 0) {
        if (HasExtension(file, L".png")) {
            return SaveWicImage(file, &GUID_ContainerFormatPng);
        }
        if (HasExtension(file, L".jpg") || HasExtension(file, L".jpeg")) {
            return SaveWicImage(file, &GUID_ContainerFormatJpeg);
        }
        return SaveWicImage(file, &GUID_ContainerFormatBmp);
    }
    return FileSaveAs(hWnd);
}


BOOL FileSaveAs(HWND hWnd)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"PNG Files (*.png)\0*.png\0"
                      L"Bitmap Files (*.bmp)\0*.bmp\0"
                      L"JPEG Files (*.jpg)\0*.jpg\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        if (!HasExtension(ofn.lpstrFile, L".png") &&
            !HasExtension(ofn.lpstrFile, L".bmp") &&
            !HasExtension(ofn.lpstrFile, L".jpg") &&
            !HasExtension(ofn.lpstrFile, L".jpeg")) {
            if (ofn.nFilterIndex == 1) {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, L".png");
            } else if (ofn.nFilterIndex == 3) {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, L".jpg");
            } else {
                EnsureExtensionForSave(ofn.lpstrFile, MAX_PATH, L".bmp");
            }
        }

        if (HasExtension(ofn.lpstrFile, L".png")) {
            if (SaveWicImage(ofn.lpstrFile, &GUID_ContainerFormatPng)) {
                Doc_SetFile(ofn.lpstrFile);
                return TRUE;
            }
            return FALSE;
        }

        if (HasExtension(ofn.lpstrFile, L".jpg") || HasExtension(ofn.lpstrFile, L".jpeg")) {
            if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_JPEG_OPTIONS), hWnd, JpegOptionsDlgProc) == IDCANCEL) {
                return FALSE;
            }
            if (SaveWicImage(ofn.lpstrFile, &GUID_ContainerFormatJpeg)) {
                Doc_SetFile(ofn.lpstrFile);
                return TRUE;
            }
            return FALSE;
        }

        if (SaveWicImage(ofn.lpstrFile, &GUID_ContainerFormatBmp)) {
            Doc_SetFile(ofn.lpstrFile);
            return TRUE;
        }
        return FALSE;
    }
    DWORD dlgErr = CommDlgExtendedError();
    if (dlgErr != 0) {
        wchar_t msg[128];
        StringCchPrintfW(msg, 128, L"Save dialog failed (0x%08lX).", dlgErr);
        MessageBoxW(hWnd, msg, L"File Save Error", MB_ICONERROR);
    }
    return FALSE;
}
