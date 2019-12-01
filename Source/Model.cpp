#include "Model.hpp"



ModelManager::ModelManager()
{
	mVertices.reserve(10'000);
	mIndices.reserve(10'000);
}

void ModelManager::addMesh(aiMesh* mesh)
{
	const auto indexStart = mIndices.size();
	const auto vertexStart = mVertices.size();

	mVertices.reserve(mesh->mNumVertices + vertexStart);
	mIndices.reserve(mesh->mNumFaces * 3 + indexStart);
	
	mVertices.insert(mVertices.end(), (Vertex*)mesh->mVertices, (Vertex*)mesh->mVertices + mesh->mNumVertices);

	for (size_t i = 0; i < mesh->mNumFaces; ++i)
		mIndices.insert(mIndices.end(), mesh->mFaces[i].mIndices, mesh->mFaces[i].mIndices + 3);

	for (size_t i = indexStart; i < mIndices.size(); ++i)
		mIndices[i] += vertexStart;
}

void ModelManager::addPlane(Vertex* vertices)
{
	const auto vertexStart = mVertices.size();

	uint32_t indices[] = { 0, 1, 2, 0, 2, 3, };

	mVertices.insert(mVertices.end(), vertices, vertices + 4);

	for (const auto& i : indices)
		mIndices.emplace_back(i + vertexStart);
}

void ModelManager::createBuffers(ID3D11Device& device)
{
	D3D11_BUFFER_DESC vertexBufferDescriptor = {};
	vertexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDescriptor.StructureByteStride = sizeof(Vertex);
	vertexBufferDescriptor.ByteWidth = sizeof(Vertex) * mVertices.size();
	vertexBufferDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	vertexBufferDescriptor.CPUAccessFlags = 0;
	vertexBufferDescriptor.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData = {};
	vertexBufferData.pSysMem = mVertices.data();

	D3D11_BUFFER_DESC indexBufferDescriptor = {};
	indexBufferDescriptor.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDescriptor.StructureByteStride = sizeof(uint32_t);
	indexBufferDescriptor.ByteWidth = sizeof(uint32_t) * mIndices.size();
	indexBufferDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	indexBufferDescriptor.CPUAccessFlags = 0;
	indexBufferDescriptor.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA indexBufferData = {};
	indexBufferData.pSysMem = mIndices.data();

	device.CreateBuffer(&vertexBufferDescriptor, &vertexBufferData, &mVertexBuffer);
	device.CreateBuffer(&indexBufferDescriptor, &indexBufferData, &mIndexBuffer);


	D3D11_SHADER_RESOURCE_VIEW_DESC vertexSRVDesc = {};
	vertexSRVDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	vertexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	vertexSRVDesc.Buffer.FirstElement = 0;
	vertexSRVDesc.Buffer.NumElements = mVertices.size();

	D3D11_SHADER_RESOURCE_VIEW_DESC indexSRVDesc = {};
	indexSRVDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
	indexSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	indexSRVDesc.Buffer.FirstElement = 0;
	indexSRVDesc.Buffer.NumElements = mIndices.size() / 3;

	device.CreateShaderResourceView(mVertexBuffer, &vertexSRVDesc, &mVertexSRV);
	device.CreateShaderResourceView(mIndexBuffer, &indexSRVDesc, &mIndexSRV);
}

