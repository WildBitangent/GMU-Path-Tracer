#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <utility>

#include "UniqueDX11.hpp"
#include "assimp/Importer.hpp"
#include "Model.hpp"
#include "Camera.hpp"
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
	void initScene();
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
	// Camera mCamera;
	// ModelManager mModel;
	// uint32_t mNumIndices;
	

	uni::Swapchain mSwapChain;
	uni::Device mDevice;
	uni::DeviceContext mContext;
	uni::RenderTargetView mRenderTarget;
	
	// uni::Buffer mVertexBuffer;
	// uni::Buffer mIndexBuffer;
	uni::VertexShader mVertexShader;
	uni::PixelShader mPixelShader;
	uni::ComputeShader mShaderLogic;
	uni::ComputeShader mShaderNewPath;
	uni::ComputeShader mShaderMaterialUE4;
	uni::ComputeShader mShaderMaterialGlass;
	uni::ComputeShader mShaderExtensionRay;
	uni::ComputeShader mShaderShadowRay;
	
	uni::InputLayout mVertexLayout;

	// uni::DepthStencilView mDepthStencil;
	uni::Texure2D mRenderTexture;
	uni::ShaderResourceView mRenderTextureSRV;
	uni::UnorderedAccessView mRenderTextureUAV;
	
	Buffer mLightBuffer;
	
	uni::Buffer mCameraBuffer;
	uni::Buffer mPathStateBuffer;
	uni::Buffer mQueueBuffer;
	uni::Buffer mQueueCountersBuffer;

	uni::UnorderedAccessView mPathStateUAV;
	uni::UnorderedAccessView mQueueUAV;
	uni::UnorderedAccessView mQueueCountersUAV;

	// uni::SamplerState mSampler;
	// uni::ShaderResourceView mTexture;
	//
	friend class GUI;
};
