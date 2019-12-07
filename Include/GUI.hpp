#pragma once
#include <Windows.h>
#include <d3d11.h>

class Renderer;

class GUI
{
public:
	GUI(Renderer& renderer);
	~GUI();

	void init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
	void update();
	void render() const;

private:
	Renderer& mRenderer;
	int mPickedResolution = 0;
	
};
