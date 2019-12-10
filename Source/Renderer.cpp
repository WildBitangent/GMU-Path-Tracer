#define FMT_HEADER_ONLY

#include "Renderer.hpp"
#include "Constants.hpp"
#include "fmt.h"
#include "assimp/postprocess.h"

#include <d3dcompiler.h>
#include <stdexcept>
#include <vector>
#include <array>
#include "Input.hpp"
#include "nvapi/nvapi.h"
#include <thread>
#include "lodepng/lodepng.h"
#include <filesystem>

namespace fs = std::filesystem;
using namespace DirectX;

Renderer::Renderer(HWND hwnd, Resolution resolution)
	: mGUI(*this)
	, mHwnd(hwnd)
{
	NvAPI_Initialize();
	createDevice(hwnd, resolution);
	createRenderTexture({ WIDTH, HEIGHT });
	
	createBuffers();

	mVertexShader = createShader<uni::VertexShader>(LR"(Assets\Shaders\Shader.vs.hlsl)", "vs_5_0");
	mPixelShader = createShader<uni::PixelShader>(LR"(Assets\Shaders\Shader.ps.hlsl)", "ps_5_0");

	reloadComputeShaders();

	mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = WIDTH;
	viewport.Height = HEIGHT;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	mContext->RSSetViewports(1, &viewport);
	
	initScene(DEFAULT_SCENE);
	
	mGUI.init(hwnd, mDevice, mContext);
}

void Renderer::initScene(const std::string& name)
{
	mScene = Scene(mDevice, std::string(R"(Assets\Models\)" + name));
}

void Renderer::createBuffers()
{
	D3D11_BUFFER_DESC cameraBufferDescriptor = {};
	cameraBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
	cameraBufferDescriptor.ByteWidth = sizeof(Camera::CameraBuffer);
	cameraBufferDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cameraBufferDescriptor.CPUAccessFlags = 0;
	cameraBufferDescriptor.MiscFlags = 0;

	mDevice->CreateBuffer(&cameraBufferDescriptor, nullptr, &mCameraBuffer);
	
	D3D11_BUFFER_DESC pathStateDescriptor = {};
	pathStateDescriptor.Usage = D3D11_USAGE_DEFAULT;
	pathStateDescriptor.ByteWidth = PATHCOUNT * 244;
	pathStateDescriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	pathStateDescriptor.CPUAccessFlags = 0;
	pathStateDescriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	D3D11_BUFFER_DESC queueDescriptor = {};
	queueDescriptor.Usage = D3D11_USAGE_DEFAULT;
	queueDescriptor.ByteWidth = PATHCOUNT * 20;
	queueDescriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	queueDescriptor.CPUAccessFlags = 0;
	queueDescriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

	D3D11_BUFFER_DESC queueCountersDescriptor = {};
	queueCountersDescriptor.Usage = D3D11_USAGE_DEFAULT;
	queueCountersDescriptor.ByteWidth = 32;
	queueCountersDescriptor.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	queueCountersDescriptor.CPUAccessFlags = 0;
	queueCountersDescriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	
	mDevice->CreateBuffer(&pathStateDescriptor, nullptr, &mPathStateBuffer);
	mDevice->CreateBuffer(&queueDescriptor, nullptr, &mQueueBuffer);
	mDevice->CreateBuffer(&queueCountersDescriptor, nullptr, &mQueueCountersBuffer);

	// views
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDescriptor = {};
	UAVDescriptor.Format = DXGI_FORMAT_R32_TYPELESS;
	UAVDescriptor.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
	UAVDescriptor.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	
	UAVDescriptor.Buffer.NumElements = pathStateDescriptor.ByteWidth / 4;
	mDevice->CreateUnorderedAccessView(mPathStateBuffer, &UAVDescriptor, &mPathStateUAV);

	UAVDescriptor.Buffer.NumElements = queueDescriptor.ByteWidth / 4;
	mDevice->CreateUnorderedAccessView(mQueueBuffer, &UAVDescriptor, &mQueueUAV);

	UAVDescriptor.Buffer.NumElements = 8;
	mDevice->CreateUnorderedAccessView(mQueueCountersBuffer, &UAVDescriptor, &mQueueCountersUAV);
}

void Renderer::createRenderTexture(Resolution res)
{
	D3D11_TEXTURE2D_DESC renderTextureDescriptor = {};
	renderTextureDescriptor.Width = res.first;
	renderTextureDescriptor.Height = res.second;
	renderTextureDescriptor.MipLevels = 1;
	renderTextureDescriptor.ArraySize = 2;
	renderTextureDescriptor.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	renderTextureDescriptor.SampleDesc.Count = 1;
	renderTextureDescriptor.SampleDesc.Quality = 0;
	renderTextureDescriptor.Usage = D3D11_USAGE_DEFAULT;
	renderTextureDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	renderTextureDescriptor.CPUAccessFlags = 0;
	renderTextureDescriptor.MiscFlags = 0;

	D3D11_SHADER_RESOURCE_VIEW_DESC renderTextureSRVDesc = {};
	renderTextureSRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	renderTextureSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	renderTextureSRVDesc.Texture2DArray.ArraySize = 2; // only first texture needed for rendering
	renderTextureSRVDesc.Texture2DArray.MipLevels = 1;

	D3D11_UNORDERED_ACCESS_VIEW_DESC renderTextureUAVDesc = {};
	renderTextureUAVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	renderTextureUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
	renderTextureUAVDesc.Texture2DArray.ArraySize = 2;

	mRenderTexture.reset();
	mRenderTextureSRV.reset();
	mRenderTextureUAV.reset();
	mDevice->CreateTexture2D(&renderTextureDescriptor, nullptr, &mRenderTexture);
	mDevice->CreateShaderResourceView(mRenderTexture, &renderTextureSRVDesc, &mRenderTextureSRV);
	mDevice->CreateUnorderedAccessView(mRenderTexture, &renderTextureUAVDesc, &mRenderTextureUAV);
}

void Renderer::update(float dt)
{
	if (Input::getInstance().hasResized())
		resize(Input::getInstance().getResolution());
	
	if (Input::getInstance().keyActive('R'))
	{
		reloadComputeShaders();
		mScene.mCamera.getBuffer()->iterationCounter = -1; // will be updated in camera to the value 0
	}

	if (Input::getInstance().keyActive('C'))
		captureScreen();
	
	mScene.update(dt);
	mGUI.update();
	
	mContext->UpdateSubresource(mCameraBuffer, 0, nullptr, mScene.mCamera.getBuffer(), 0, 0);
}

void Renderer::draw()
{
	std::array<ID3D11Buffer*, 2> uniforms = { mCameraBuffer, mScene.mMaterialPropertyBuffer };
	std::array<ID3D11ShaderResourceView*, 8> SRVs = {
		mScene.mBVHBuffer.srv,
		mScene.mIndexBuffer.srv,
		mScene.mVertexBuffer.srv,
		mScene.mLightBuffer.srv,
		mScene.mTriangleProperties.srv,
		mScene.mDiffuse.srv,
		mScene.mMetallicRoughness.srv,
		mScene.mNormal.srv,
	};
	std::array<ID3D11UnorderedAccessView*, 4> UAVs = {
		mRenderTextureUAV,
		mPathStateUAV,
		mQueueUAV,
		mQueueCountersUAV,
	};
	std::array<ID3D11ShaderResourceView*, SRVs.max_size()> nullSRV = {};
	std::array<ID3D11UnorderedAccessView*, UAVs.max_size()> nullUAV = {};
	
	mContext->VSSetShader(mVertexShader, nullptr, 0);
	mContext->PSSetShader(mPixelShader, nullptr, 0);

	mContext->CSSetShaderResources(0, SRVs.size(), SRVs.data());
	mContext->CSSetConstantBuffers(0, uniforms.size(), uniforms.data());
	mContext->CSSetUnorderedAccessViews(0, UAVs.size(), UAVs.data(), nullptr);
	mContext->CSSetSamplers(0, 1, &mScene.mSampler);
	

	mContext->CSSetShader(mShaderLogic, nullptr, 0);
	mContext->Dispatch(512, 1, 1);
	
	mContext->CSSetShader(mShaderNewPath, nullptr, 0);
	mContext->Dispatch(512, 1, 1);
	
	mContext->CSSetShader(mShaderMaterialUE4, nullptr, 0); // TODO maybe pick less thread groups since materials will be most likely uniformly distributed
	mContext->Dispatch(512, 1, 1);
	
	mContext->CSSetShader(mShaderMaterialGlass, nullptr, 0);
	mContext->Dispatch(512, 1, 1); 

	mContext->CSSetShader(mShaderExtensionRay, nullptr, 0);
	mContext->Dispatch(512, 1, 1);
	
	mContext->CSSetShader(mShaderShadowRay, nullptr, 0);
	mContext->Dispatch(512, 1, 1);

	
	mContext->CSSetShaderResources(0, SRVs.size(), nullSRV.data());
	mContext->CSSetUnorderedAccessViews(0, UAVs.size(), nullUAV.data(), nullptr);
	
	mContext->ClearRenderTargetView(mRenderTarget, std::array<float, 4>({ 0, 0, 0, 0.0f }).data());

	mContext->PSSetSamplers(0, 1, &mScene.mSampler);
	mContext->PSSetShaderResources(0, 1, &mRenderTextureSRV);

	mContext->Draw(4, 0);
	mContext->PSSetShaderResources(0, 1, nullSRV.data());

	mGUI.render();
	
	mSwapChain->Present(0, 0);

}

IDXGIAdapter* Renderer::enumerateDevice()
{
	IDXGIFactory* pFactory;
	CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pFactory));

	IDXGIAdapter* adapter;
	DXGI_ADAPTER_DESC adapterDesc = {};
	
	for (size_t i = 0; pFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapter->GetDesc(&adapterDesc);

		if (adapterDesc.VendorId == 0x10DE) // Nvidia GPU
			return adapter;
	}

	pFactory->Release();

	return nullptr;
}

void Renderer::createDevice(HWND hwnd, Resolution resolution)
{
	DXGI_MODE_DESC bufferDesc = {};
	bufferDesc.Width = resolution.first;
	bufferDesc.Height = resolution.second;
	bufferDesc.RefreshRate.Numerator = 60;
	bufferDesc.RefreshRate.Denominator = 1;
	bufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	bufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferDesc = bufferDesc;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.OutputWindow = hwnd;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; 

	auto result = D3D11CreateDeviceAndSwapChain(
		enumerateDevice(), 
		D3D_DRIVER_TYPE_UNKNOWN, 
		nullptr, 
		D3D11_CREATE_DEVICE_DEBUG,  // TODO remove debug
		nullptr, 0,
		D3D11_SDK_VERSION, 
		&swapChainDesc, &mSwapChain, 
		&mDevice, 
		nullptr, 
		&mContext
	);
	
	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to create device with Swapchain. ERR: {}", result));

	ID3D11Texture2D* backBuffer;
	result = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to create Back Buffer. ERR: {}", result));

	result =  mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTarget);
	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to create Render Target View. ERR: {}", result));

	mContext->OMSetRenderTargets(1, &mRenderTarget, nullptr);
	backBuffer->Release();
}

void Renderer::reloadComputeShaders()
{
	if (NvAPI_D3D11_SetNvShaderExtnSlot(mDevice, 5) != NVAPI_OK)
		throw std::runtime_error("Failed to add Nv Extension.");

	std::exception_ptr deferredException;
	auto work = [this, &deferredException](uni::ComputeShader& shader, const std::wstring& path)
	{
		try
		{
			shader = createShader<uni::ComputeShader>(path, "cs_5_0");
		}
		catch(...)
		{
			deferredException = std::current_exception();
		}
	};
	
	std::vector<std::pair<uni::ComputeShader&, std::wstring>> shaders = {
		{ mShaderLogic, LR"(Assets\Shaders\logic.hlsl)" },
		{ mShaderNewPath, LR"(Assets\Shaders\newPath.hlsl)" },
		{ mShaderMaterialUE4, LR"(Assets\Shaders\materialUE4.hlsl)" },
		{ mShaderMaterialGlass, LR"(Assets\Shaders\materialGlass.hlsl)" },
		{ mShaderExtensionRay, LR"(Assets\Shaders\extensionRayCast.hlsl)" },
		{ mShaderShadowRay, LR"(Assets\Shaders\shadowRayCast.hlsl)" },
	};
	
	std::vector<std::thread> workers;

	for (auto& p : shaders)
		workers.emplace_back(work, std::ref(p.first), std::ref(p.second));

	for (auto& t : workers)
		t.join();

	try
	{
		if (deferredException)
			std::rethrow_exception(deferredException);
	}
	catch(std::runtime_error& e)
	{
		for (auto& s : shaders)
			if (!s.first)
				throw;
		
		OutputDebugString(fmt::format("Failed to compile shader. Continuing with last working version.\n {}", e.what()).c_str());
	}

	NvAPI_D3D11_SetNvShaderExtnSlot(mDevice, ~0u);
}

void Renderer::captureScreen()
{
	auto resolution = Input::getInstance().getResolution();
    D3D11_TEXTURE2D_DESC desc = {};
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Width = resolution.first;
    desc.Height = resolution.second;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

     uni::Texure2D texture;
     if (mDevice->CreateTexture2D(&desc, nullptr, &texture) != S_OK)
		 return;

	mContext->CopySubresourceRegion(texture, 0, 0, 0, 0, mRenderTexture, 0, nullptr);
	
	D3D11_MAPPED_SUBRESOURCE subresource;
	mContext->Map(texture, 0, D3D11_MAP_READ, {}, &subresource);

	resolution.first = subresource.RowPitch / 16;
	resolution.second = (subresource.DepthPitch / subresource.RowPitch);
	
	std::vector<unsigned char> image(resolution.first * resolution.second * 4);
	for (size_t i = 0; i < resolution.first * resolution.second * 4; i+= 4)
	{
		image[i] = reinterpret_cast<float*>(subresource.pData)[i] * 255;
		image[i + 1] = reinterpret_cast<float*>(subresource.pData)[i + 1] * 255;
		image[i + 2] = reinterpret_cast<float*>(subresource.pData)[i + 2] * 255;
		image[i + 3] = 255;
	}


	auto counter = -1;
	if (fs::exists(CAPTURE_DIR_NAME))
	{
	    for (const auto & entry : fs::directory_iterator(CAPTURE_DIR_NAME))
	    {
	    	auto numstr = entry.path().filename().string().substr(strlen(CAPTURE_NAME));
	    	counter = std::max(counter, atoi(numstr.c_str()));
	    }
	}
	else
		CreateDirectory(CAPTURE_DIR_NAME, nullptr);

	lodepng::encode(fmt::format(R"({}\{}{}.png)", CAPTURE_DIR_NAME, CAPTURE_NAME, counter + 1), image, resolution.first, resolution.second);
	mContext->Unmap(texture, 0);
}

void Renderer::resize(const Resolution& resolution)
{
	mScene.mCamera.updateResolution(resolution.first, resolution.second);
	resizeSwapchain(resolution);
	createRenderTexture(resolution);
}

void Renderer::resizeSwapchain(const Resolution& resolution)
{
	mRenderTarget.reset();
	mContext->OMSetRenderTargets(0, nullptr, nullptr);
	mSwapChain->ResizeBuffers(1, resolution.first, resolution.second, DXGI_FORMAT_R8G8B8A8_UNORM, {});

	ID3D11Texture2D* backBuffer;
	auto result = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer));

	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to create Back Buffer. ERR: {}", result));

	result =  mDevice->CreateRenderTargetView(backBuffer, nullptr, &mRenderTarget);
	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to create Render Target View. ERR: {}", result));

	mContext->OMSetRenderTargets(1, &mRenderTarget, nullptr);
	backBuffer->Release();

	D3D11_VIEWPORT viewport;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = resolution.first;
	viewport.Height = resolution.second;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	mContext->RSSetViewports(1, &viewport);
}

void Renderer::initResize(Resolution res)
{
	RECT currentWindowRect;
	GetWindowRect(mHwnd, &currentWindowRect);

	RECT currentClientSize = { 0, 0, res.first, res.second };
	AdjustWindowRect(&currentClientSize, WS_OVERLAPPEDWINDOW, false);

	MoveWindow(mHwnd, 
		currentWindowRect.left, currentWindowRect.top, 
		currentClientSize.right - currentClientSize.left, 
		currentClientSize.bottom - currentClientSize.top, 
		false
	);
}

template<typename T>
T Renderer::createShader(const std::wstring& path, const std::string& target)
{
	T shader;
	uni::Blob err;
	uni::Blob compiled;
	
	auto result = D3DCompileFromFile(path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", target.c_str(), {}, {}, &compiled, &err);
	if (result != S_OK)
		throw std::runtime_error(fmt::format("Failed to compile {}. ERR: {}\n\n{}", std::string(path.begin(), path.end()), result, reinterpret_cast<const char*>(err->GetBufferPointer())));

	if constexpr (std::is_same_v<T, uni::VertexShader>)
		result = mDevice->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &shader);
	else if constexpr (std::is_same_v<T, uni::PixelShader>)
		result = mDevice->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &shader);
	else if constexpr (std::is_same_v<T, uni::ComputeShader>)
		result = mDevice->CreateComputeShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &shader);
	else
		static_assert("Unsupported shader.");

	if (result != S_OK)
		throw std::runtime_error("Failed to create shader. ");
	
	return std::move(shader);
}
