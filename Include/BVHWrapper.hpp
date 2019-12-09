#pragma once
#include <vector>
#include <DirectXMath.h>
#include "Nvidia-SBVH/Array.h"

struct aiMesh;
struct aiScene;


class BVHWrapper
{
public:
	struct alignas(16) BVHNode 
	{
		DirectX::XMFLOAT3A min;
		DirectX::XMFLOAT3A max;
		int leftIndex;
		int rightIndex;
		int isLeaf;
		
	}; 

	struct alignas(16) Triangle
	{
		DirectX::XMINT3 indices;
		uint32_t index; // triangle index (for material)
	};
	
	struct alignas(16) TriangleProperties
	{
		DirectX::XMFLOAT3A normal;
		DirectX::XMFLOAT2 texCoord;
		uint32_t materialID;
	};
	
public:
	BVHWrapper() = default;
	BVHWrapper(const aiScene* scene);

private:
	void buildSBVH();

private:
	const aiScene* mScene;
	std::vector<BVHNode> mGPUTree;
	std::vector<Triangle> mIndices;
	std::vector<TriangleProperties> mTriangleProperties;
    Array<Vec3f> mVertices;

	friend class Scene; // todo lazy to make getters/setters :'(
};