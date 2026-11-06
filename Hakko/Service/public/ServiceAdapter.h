#pragma once

#include "DataHub.h"


namespace Hakko
{

class ServiceAdapter
{
    
public:
    ServiceAdapter() = default;
    virtual ~ServiceAdapter() = default;


public:
	virtual bool Init(int width, int height) = 0;
	virtual void Finalize() = 0;

	virtual void OnResize(int width, int height) = 0;
	
	virtual void Update() = 0;
	virtual void OnRender() = 0;


    virtual void OnMouseMove(int x, int y) = 0;

    virtual void OnKeyDown(int keyCode) = 0;
    virtual void OnKeyUp(int keyCode) = 0;

};

}
