#pragma once
#include <utility>
#include "UniqueDX11.hpp"

struct Buffer
{
	uni::Buffer buffer;
	uni::ShaderResourceView srv;
};

struct Texture
{
	uni::Texure2D texture;
	uni::ShaderResourceView srv;
}; 

template <typename T>
Buffer createBuffer(ID3D11Device* device, unsigned stride, const T& data, DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, UINT flags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED)
{
	Buffer buffer;
	
	D3D11_BUFFER_DESC bufferDescriptor = {};
	bufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
	bufferDescriptor.StructureByteStride = stride;
	bufferDescriptor.ByteWidth = stride * data.size();
	bufferDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE; // D3D11_BIND_UNORDERED_ACCESS
	bufferDescriptor.CPUAccessFlags = 0;
	bufferDescriptor.MiscFlags = flags;

	D3D11_SUBRESOURCE_DATA bufferData = {};
	bufferData.pSysMem = data.data();
	
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDescriptor = {};
	srvDescriptor.Format = format;
	srvDescriptor.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDescriptor.Buffer.FirstElement = 0;
	srvDescriptor.Buffer.NumElements = data.size();
	
	device->CreateBuffer(&bufferDescriptor, &bufferData, &buffer.buffer);
	device->CreateShaderResourceView(buffer.buffer, &srvDescriptor, &buffer.srv);

	return buffer;
}
