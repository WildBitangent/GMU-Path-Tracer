#pragma once
#include <d3d11.h>


namespace uni
{
	template <typename T>
	class UniqueHandle
	{
	public:
		explicit UniqueHandle(T* const& value = nullptr)
		: mValue(value)
		{}

		UniqueHandle(UniqueHandle const& ) = delete;

		UniqueHandle(UniqueHandle&& other) noexcept
		: mValue(other.release())
		{}

		~UniqueHandle()
		{
			if (mValue) this->destroy();
		}

		UniqueHandle& operator=(UniqueHandle const&) = delete;

		UniqueHandle& operator=(UniqueHandle&& other) noexcept
		{
			reset(other.release());
			return *this;
		}

		explicit operator bool() const
		{
			return mValue;
		}

		T const* operator->() const
		{
			return mValue;
		}

		T* operator->()
		{
			return mValue;
		}

		T const& operator*() const
		{
			return *mValue;
		}

		T& operator*()
		{
			return *mValue;
		}
		
		T** operator&()
		{
			return &mValue;
		}

		operator T*()
		{
			return mValue;
		}

		void reset(T* const& value = nullptr)
		{
			if (mValue != value)
			{
				if (mValue) this->destroy();
				mValue = value;
			}
		}

		T* release() noexcept
		{
			T* value = mValue;
			mValue = nullptr;
			return value;
		}

		void destroy()
		{
			mValue->Release();
		}

	private:
		T* mValue;
	};

	using Swapchain = UniqueHandle<IDXGISwapChain>;
	using Factory = UniqueHandle<IDXGIFactory>;
	using Device = UniqueHandle<ID3D11Device>;
	using DeviceContext = UniqueHandle<ID3D11DeviceContext>;
	using RenderTargetView = UniqueHandle<ID3D11RenderTargetView>;
	using Buffer = UniqueHandle<ID3D11Buffer>;
	using Texure2D = UniqueHandle<ID3D11Texture2D>;
	using VertexShader = UniqueHandle<ID3D11VertexShader>;
	using PixelShader = UniqueHandle<ID3D11PixelShader>;
	using ComputeShader = UniqueHandle<ID3D11ComputeShader>;
	using Blob = UniqueHandle<ID3DBlob>;
	using InputLayout = UniqueHandle<ID3D11InputLayout>;
	using DepthStencilView = UniqueHandle<ID3D11DepthStencilView>;
	using SamplerState = UniqueHandle<ID3D11SamplerState>;
	using ShaderResourceView = UniqueHandle<ID3D11ShaderResourceView>;
	using UnorderedAccessView = UniqueHandle<ID3D11UnorderedAccessView>;
}
