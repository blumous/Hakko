#pragma once

#include "ServiceAdapter.h"

#include <windows.h>


namespace Hakko
{

class GameService : public ServiceAdapter
{
    
public:
    GameService();
    ~GameService() = default;


public:
	bool Init(int width, int height) override;
	void Finalize() override;

	void OnResize(int width, int height) override;

	void Update() override;
	void OnRender() override;


    void OnMouseMove(int x, int y) override;

    void OnKeyDown(int keyCode) override;
    void OnKeyUp(int keyCode) override;

};


enum class KeyCode
{
    Mouse0 = VK_LBUTTON,
    Mouse1 = VK_RBUTTON,
    Escape = VK_ESCAPE,
    Space = VK_SPACE,
    UpArrow = VK_UP,
    LeftArrow = VK_LEFT,
    DownArrow = VK_DOWN,
    RightArrow = VK_RIGHT,
    W = 'W',
    A = 'A',
    S = 'S',
    D = 'D',
    Q = 'Q',
    E = 'E',
    R = 'R',
    F = 'F',
    Z = 'Z',
    X = 'X',
    C = 'C',
    V = 'V',
    Num1 = '1',
    Num2 = '2',
    Num3 = '3',
    Num4 = '4',
    Num5 = '5',
    Num6 = '6',
    Num7 = '7',
    Num8 = '8',
    Num9 = '9',
    Num0 = '0',
};

}
