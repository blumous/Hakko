#include "WinAdapter.h"

#include "DataHub.h"

#include "GameService.h"
#include "RenderFactory.h"


using namespace Hakko;


bool WinAdapter::Init(HWND hWnd, int width, int height)
{
    if (!DataHub::Init())
    {
        OutputDebugStringW(L"[Init] DataHub::Init 실패\n");
        return false;
    }

    serviceCore = new GameService();
    renderCore = CreateRenderer(RendererDesc{});


    if (!serviceCore->Init(width, height))
    {
        OutputDebugStringW(L"[Init] serviceCore->Init 실패\n");
        return false;
    }
    if (!renderCore->Init(hWnd, width, height))
    {
        OutputDebugStringW(L"[Init] renderCore->Init 실패\n");
        return false;
    }

    return true;
}

void WinAdapter::Exit()
{
    renderCore->Finalize();
    serviceCore->Finalize();

    DataHub::Finalize();
    
    delete serviceCore;
    DestroyRenderer(renderCore);
}

void WinAdapter::OnResize(int width, int height)
{
    if (serviceCore)
        serviceCore->OnResize(width, height);
    if (renderCore)
        renderCore->OnResize(width, height);
}


void WinAdapter::OnWndInput(MSG msg)
{
    if (msg.message == WM_MOUSEMOVE)
    {
        serviceCore->OnMouseMove(LOWORD(msg.lParam), HIWORD(msg.lParam));
    }
    else if (msg.message == WM_LBUTTONDOWN)
    {
        serviceCore->OnKeyDown(VK_LBUTTON);
    }
    else if (msg.message == WM_LBUTTONUP)
    {
        serviceCore->OnKeyUp(VK_LBUTTON);
    }
    else if (msg.message == WM_RBUTTONDOWN)
    {
        serviceCore->OnKeyDown(VK_RBUTTON);
    }
    else if (msg.message == WM_RBUTTONUP)
    {
        serviceCore->OnKeyUp(VK_RBUTTON);
    }

    else if (msg.message == WM_KEYDOWN)
    {
        serviceCore->OnKeyDown(msg.wParam);
    }
    else if (msg.message == WM_KEYUP)
    {
        serviceCore->OnKeyUp(msg.wParam);
    }
}


void WinAdapter::FrameUpdate()
{

    Update();
    Render();
}


void WinAdapter::Update()
{
    serviceCore->Update();
    renderCore->Sketch();
}

void WinAdapter::Render()
{
    serviceCore->OnRender();
    renderCore->Render();
}
