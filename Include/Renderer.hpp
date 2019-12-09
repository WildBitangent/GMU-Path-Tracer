#pragma once
#include <d3d11.h>
#include <utility>

#include "UniqueDX11.hpp"
#include "Scene.hpp"
#include "GUI.hpp"

class Renderer
{
	using Resolution = std::pair<unsigned, unsigned>;
public:
	Renderer(HWND hwnd, Resolution resolution);

	void update(float dt);
	void draw();

private:
	IDXGIAdapter* enumerateDevice();
	void createDevice(HWND hwnd, Resolution resolution);
	void initScene(const std::string& name);
	void createBuffers();
	void createRenderTexture(Resolution res);
	void reloadComputeShaders(); // TODO rewrite
	void captureScreen();
	void resize(const Resolution& resolution);
	void resizeSwapchain(const Resolution& resolution);
	void initResize(Resolution res);

	template<typename T>
	T createShader(const std::wstring& path, const std::string& target);

private:
	HWND mHwnd;
	GUI mGUI;
	Scene mScene;	

	uni::Swapchain mSwapChain;
	uni::Device mDevice;
	uni::DeviceContext mContext;
	uni::RenderTargetView mRenderTarget;
	
	uni::VertexShader mVertexShader;
	uni::PixelShader mPixelShader;
	uni::ComputeShader mShaderLogic;
	uni::ComputeShader mShaderNewPath;
	uni::ComputeShader mShaderMaterialUE4;
	uni::ComputeShader mShaderMaterialGlass;
	uni::ComputeShader mShaderExtensionRay;
	uni::ComputeShader mShaderShadowRay;
	
	uni::InputLayout mVertexLayout;

	uni::Texure2D mRenderTexture;
	uni::ShaderResourceView mRenderTextureSRV;
	uni::UnorderedAccessView mRenderTextureUAV;
	
	uni::Buffer mCameraBuffer;
	uni::Buffer mPathStateBuffer;
	uni::Buffer mQueueBuffer;
	uni::Buffer mQueueCountersBuffer;

	uni::UnorderedAccessView mPathStateUAV;
	uni::UnorderedAccessView mQueueUAV;
	uni::UnorderedAccessView mQueueCountersUAV;

	friend class GUI;
};
