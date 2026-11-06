#pragma once

#include <windows.h>

#include "WinAdapter.h"


inline int width = 1280;
inline int height = 960;

HINSTANCE hInst = nullptr;

WinAdapter  winCore;



ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    wchar_t exePath[MAX_PATH] = {};
    const DWORD exePathLen = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (exePathLen > 0 && exePathLen < MAX_PATH)
    {
        wchar_t* slash = wcsrchr(exePath, L'\\');
        if (slash != nullptr)
        {
            *slash = L'\0';
            SetCurrentDirectoryW(exePath);
        }
    }
    
    g_width = width;
    g_height = height;
    

    ATOM classId = MyRegisterClass(hInstance);

    if (classId == 0)
    {
        return FALSE;
    }


    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }


    MSG msg = { 0 };


    if (!winCore.Init(g_hWnd, g_width, g_height))
        return -1;


    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if ((WM_MOUSEFIRST <= msg.message && msg.message <= WM_MOUSELAST)
                || (WM_KEYFIRST <= msg.message && msg.message <= WM_KEYUP))
            {
                winCore.OnWndInput(msg);
            }

            else
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            winCore.FrameUpdate();
        }
    }


    winCore.Exit();


    return (int)msg.wParam;
}



ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEXW);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = nullptr;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = g_wndClass;
    wcex.hIconSm = nullptr;

    return RegisterClassExW(&wcex);
}



BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;

    RECT rect = { 0,0,g_width,g_height };
    AdjustWindowRect(&rect, dwStyle, FALSE);


    g_hWnd = CreateWindowW(g_wndClass, g_wndTitle, dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, hInstance, nullptr);


    if (!g_hWnd)
        return FALSE;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    return TRUE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        g_width = width;
        g_height = height;
        winCore.OnResize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        DestroyWindow(hWnd);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

