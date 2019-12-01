#pragma once
#include "UniqueDX11.hpp"
#include "assimp/scene.h"

#include <DirectXMath.h>
#include <vector>


struct Vertex
{
	DirectX::XMFLOAT3 position;
	// XMFLOAT3 normal;
};

struct DrawCall
{
	size_t indexOffset;
	size_t numVertices;
	size_t materialID;
};

struct Material
{
	DirectX::XMFLOAT3 color;
};

class ModelManager
{
	friend class Renderer;
	
public:
	ModelManager();


	void addMesh(aiMesh* mesh);
	void addPlane(Vertex* vertices);

	void createBuffers(ID3D11Device& device);
	
private:	
	std::vector<Vertex> mVertices;
	std::vector<uint32_t> mIndices;
	std::vector<Material> mMaterials;
	std::vector<DrawCall> mDrawCalls;
	
	uni::Buffer mVertexBuffer;
	uni::Buffer mIndexBuffer;
	
	uni::ShaderResourceView mVertexSRV;
	uni::ShaderResourceView mIndexSRV;

	

	// uint32_t mNumTriangles;
};
