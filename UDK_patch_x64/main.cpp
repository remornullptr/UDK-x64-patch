#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <fstream>

#include "resource.h"


#define BUTTON_OPEN 40
#define BUTTON_PATCH 41

#define PAINT_TIME 2000.0f

#define W 800
#define H 600

char target_path[260] = { 0 };

double last_time = 0.0;
double start_time = 0.0;
bool draw_ready = false;

HWND MainWND;
HWND ControlWND;
HWND PatchButtonWND;

HBITMAP background;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ChildProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int Patch(char *dest) 
{
    
    IMAGE_DOS_HEADER* dos = new IMAGE_DOS_HEADER;
    IMAGE_NT_HEADERS* nt = new IMAGE_NT_HEADERS;
    IMAGE_SECTION_HEADER pdata{};

    char udk_name_buffer[8]{};
    int path_len = strlen(dest);
    if (path_len >= 7) {
        memcpy(udk_name_buffer, dest + path_len - 7, 7);
        for (char* symbol = udk_name_buffer; *symbol != 0; symbol++) {
            *symbol = toupper(*symbol);
        }
    }



    if (strcmp(udk_name_buffer, "UDK.EXE") != 0)
    {
        MessageBoxA(
            MainWND,
            "EXE file name mismatch. You must select 64bit \"UDK.exe\"",
            "File mismatch",
            MB_ICONWARNING | MB_OK
        );
        return -10;
    }

    std::ifstream file(dest, std::ios::binary | std::ios::in);
    if (!file.is_open()){
        MessageBoxA(
            MainWND,
            "Can\'t open file in read mode.",
            "Read error",
            MB_ICONERROR | MB_OK
        );
        return -1;
    }

    file.read((char*)dos, sizeof(IMAGE_DOS_HEADER));

    
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) 
    {
        MessageBoxA(
            MainWND,
            "This file don\'t contain DOS header.",
            "Invalid file",
            MB_ICONERROR | MB_OK
        );
        return -10;
    }


    file.seekg(dos->e_lfanew);
    file.read((char*)nt, sizeof(IMAGE_NT_HEADERS));

    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) 
    {
        MessageBoxA(
            MainWND,
            "This is patch only for x64 arch.",
            "Invalid architecture",
            MB_ICONERROR | MB_OK
        );
        file.close();
        return -10;
    }

    DWORD sectionLocation = (DWORD)dos->e_lfanew + sizeof(DWORD) + (DWORD)(sizeof(IMAGE_FILE_HEADER)) + (DWORD)nt->FileHeader.SizeOfOptionalHeader;

    DWORD importDirectoryRVA = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        void* a = (void*)sectionLocation;
        file.seekg((unsigned long)a);
        file.read((char*)&pdata, sizeof(IMAGE_SECTION_HEADER));
        if (strcmp((char*)pdata.Name, ".data") == 0)
        {
            char* pbudd = new char[pdata.SizeOfRawData];
            file.seekg((unsigned long)pdata.PointerToRawData);
            file.read(pbudd, pdata.SizeOfRawData);
            file.close();


            for (char* i = pbudd; i < pbudd + pdata.SizeOfRawData; i += 16)
            {
                unsigned long* a = (unsigned long*)i;
                unsigned long* b = (unsigned long*)(i + 8);

                if (*b == 0x00000000000e0008 && *a == 0x0000000000000008)
                {
                    MessageBoxA(
                        MainWND,
                        "This UDK already patched.",
                        "Info",
                        MB_ICONINFORMATION | MB_OK
                    );
                    return -2;
                }
                if (*b == 0x00000000000e0004 && *a == 0x0000000000000004)
                {
                    unsigned long offset = pdata.PointerToRawData + i - pbudd;
                    const char* patch = "\x08\0\0\0\0\0\0\0\x08\0\x0E\0\0\0\0\0";

                    HANDLE patched = CreateFileA(dest, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
                    if (patched == INVALID_HANDLE_VALUE) {
                        MessageBoxA(
                            MainWND,
                            "Can\'t open exe with write mode!",
                            "Patch error",
                            MB_ICONERROR | MB_OK
                        );
                        return -3;
                    }
                    else {
                        if (SetFilePointer(patched, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
                        {
                            MessageBoxA(
                                MainWND,
                                "Can\'t jump to destenation address!",
                                "Patch error",
                                MB_ICONERROR | MB_OK
                            );
                            CloseHandle(patched);
                            return -4;
                        }

                        DWORD written;
                        if (!WriteFile(patched, patch, 16, &written, NULL)) {
                            char error_output[1024]{};
                            DWORD errorWrite = GetLastError();
                            sprintf_s(error_output, "Can't write to file\nError = %u\n", errorWrite);

                            MessageBoxA(
                                MainWND,
                                "Unable to write correct QWORDs",
                                "Write error",
                                MB_ICONERROR | MB_OK
                            );
                            CloseHandle(patched);
                            return -5;
                        }
                        CloseHandle(patched);
                        MessageBoxA(
                            MainWND,
                            "SUCCESS!!1",
                            "Probably",
                            MB_ICONINFORMATION | MB_OK
                        );
                        
                        return 0;
                    }
                }


            }
            
            break;
        }

        sectionLocation += sizeof(IMAGE_SECTION_HEADER);
    }

    MessageBoxA(
        MainWND,
        "This file don\'t contain DOS header.",
        "Invalid file",
        MB_ICONERROR | MB_OK
    );

    file.close();
    return -666;
}

void draw(HDC dc) 
{
    HDC memDC = CreateCompatibleDC(dc);
    SelectObject(memDC, background);

    if (draw_ready)
    {
        BitBlt(dc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
    }
    if (!draw_ready) 
    {
        if (last_time == 0.0) {
            start_time = clock();
        }
        last_time = clock() - start_time;

        int fill = (last_time / PAINT_TIME) * H * 2;

        if (fill % 10 == 0)
        {
            if (fill > H)
            {
                fill -= H;
                BitBlt(dc, 5, fill - 5, W, 5, memDC, 0, fill, SRCCOPY);
            }
            else {
                BitBlt(dc, 0, fill, W, 5, memDC, 0, fill, SRCCOPY);
            }
        }
        draw_ready = (last_time > PAINT_TIME);
        if (draw_ready) 
        {
            BitBlt(dc, 0, 0, W, H, memDC, 0, 0, SRCCOPY);
            SetParent(ControlWND, MainWND);
            ShowWindow(ControlWND, SW_SHOWNORMAL);
            InvalidateRect(ControlWND, NULL, TRUE);
            UpdateWindow(ControlWND);
        }
    }
    

    DeleteDC(memDC);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    const wchar_t CLASS_NAME[] = L"UDK x64 patch ^_^";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MAINWINDOW";
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass(&wc);

    WNDCLASS childwc = { };

    childwc.lpfnWndProc = ChildProc;
    childwc.hInstance = hInstance;
    childwc.lpszClassName = L"CHILDWINDOW";
    childwc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClass(&childwc);

    MainWND = CreateWindowEx(
        0,
        L"MAINWINDOW",
        CLASS_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        W, H,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (MainWND == NULL)
    {
        return 0;
    }

    ShowWindow(MainWND, nCmdShow);

    MSG msg = { };
    HDC dc = GetWindowDC(MainWND);

    background = LoadBitmap(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDB_BITMAP1)
    );





    ControlWND = CreateWindowEx(0, L"CHILDWINDOW", L"Patch menu",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
        , 0, H-200,
        300, 150, 
        NULL, NULL, hInstance, NULL);

    HWND OpenButtonWND = CreateWindow(
        L"BUTTON",
        L"Open UDK exe",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        0,
        60,
        100,
        50,
        ControlWND,
        (HMENU)BUTTON_OPEN,
        (HINSTANCE)GetWindowLongPtr(ControlWND, GWLP_HINSTANCE),
        NULL);

    PatchButtonWND = CreateWindow(
        L"BUTTON",
        L"Patch",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        100,       
        60,        
        100,       
        50,        
        ControlWND,
        (HMENU)BUTTON_PATCH,
        (HINSTANCE)GetWindowLongPtr(ControlWND, GWLP_HINSTANCE),
        NULL);

    HWND LabelWND = CreateWindowA("static", "label",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        0, 0, 300, 50,
        ControlWND, (HMENU)(501),
        (HINSTANCE)GetWindowLongPtr(ControlWND, GWLP_HINSTANCE), NULL);

    EnableWindow(PatchButtonWND, false);
    SetWindowTextA(LabelWND, "Open the x64 UDK.exe\nBy example: \n\"C:\\UDK\\Binaries\\Win64\\UDK.exe\"");
    

    while (true) 
    {
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) 
        {

            if (msg.message == WM_QUIT) 
            {
                break;
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else 
        {
            if (!draw_ready) {
                draw(dc);
            }   
        }
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rcPaint;
        rcPaint.left = 0;
        rcPaint.top = 0;
        rcPaint.right = W;
        rcPaint.bottom = H;

        FillRect(hdc, &rcPaint, (HBRUSH)(COLOR_WINDOW + COLOR_3DDKSHADOW));
        draw(hdc);

        EndPaint(hwnd, &ps);
        return 0;

    }
    return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ChildProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + COLOR_GRAYTEXT));
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_EXITSIZEMOVE:
    {
        InvalidateRect(MainWND, NULL, TRUE);
        UpdateWindow(MainWND);
        return 0;
    }
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == BUTTON_OPEN) {
            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = target_path;
            ofn.nMaxFile = sizeof(target_path);
            ofn.lpstrFilter = "EXE\0*.EXE\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = NULL;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = NULL;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileNameA(&ofn) == TRUE)
            {
                printf("New file: %s", target_path);
                EnableWindow(PatchButtonWND, true);
            }
        }
        else if (BUTTON_PATCH)
        {
            Patch(target_path);
        }
        return 0;
    }
    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}