#pragma once

#include <windows.h>


inline const wchar_t g_wndTitle[] = L"DXNoTemplate";
inline const wchar_t g_wndClass[] = L"DXNoTemplate";

inline HWND g_hWnd = nullptr;

inline int g_width;
inline int g_height;


namespace Hakko
{
	class ServiceAdapter;
	class RenderAdapter;
}


class WinAdapter
{
public:
	WinAdapter() : serviceCore(nullptr), renderCore(nullptr) {}
	~WinAdapter() = default;

public:
	bool Init(HWND hWnd, int width, int height);
	void Exit();
	
	void OnResize(int width, int height);

	void OnWndInput(MSG msg);


	void FrameUpdate();

private:
	Hakko::ServiceAdapter* serviceCore;
	Hakko::RenderAdapter* renderCore;
	
	void Update();

	void Render();

};
