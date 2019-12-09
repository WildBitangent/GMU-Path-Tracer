#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <DirectXMath.h>

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
	int mPickedScene = 3;
	int mEditingLight = 0;
	bool mShowEditor = false;

	bool mUpdating = false;
	DirectX::XMFLOAT3 mLightPos;
	DirectX::XMFLOAT3 mLightColor;
	float mLightPower = 80;
	float mLightRadius;
};
