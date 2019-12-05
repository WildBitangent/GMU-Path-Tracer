#pragma once
#include "Camera.hpp"
#include "BVHWrapper.hpp"
#include <assimp/Importer.hpp>
#include <d3d11.h>
#include "UniqueDX11.hpp"
#include "Util.hpp"
#include <assimp/material.h>
#include <mutex>


struct alignas(16) MaterialProperty // TODO CBUFFER
{
	enum Indices
	{
		DIFFUSE,
		METALLICROUGHNESS,
		NORMAL,
	};

	enum MaterialType
	{
		UE4,
		GLASS
	};

	
	DirectX::XMFLOAT4 color = {};
	float metallic;
	float roughness;
	float refractIndex;
	float transmittance; // for now, only as pad
	
	// int32_t indexDiffuse = -1;
	int32_t textureIndices[3] = { -1, -1, -1 };
	uint32_t materialType = UE4;

};

class Scene
{
	using RawTextureData = std::vector<std::vector<unsigned char>>;
	using LoadedTextures = std::pair<RawTextureData, unsigned>;
public:
	Scene() = default;
	Scene(ID3D11Device* device, const std::string& path);

	Scene(Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	Scene(Scene&& scene) = default;
	Scene& operator=(Scene&& scene) = default;

	void update(float dt);

private:
	void loadScene(const std::string& path);
	void loadTextures();
	void createBVH();
	void createSampler();
	void createPropertyBuffer(const std::vector<MaterialProperty>& data);

	LoadedTextures loadSpecificTexture(aiTextureType type, std::vector<MaterialProperty>& properties, MaterialProperty::Indices index) const;
	void createTextures(LoadedTextures rawTextures, Texture& resource);
	
private:
	// BVHWrapper mBVH;
	
	ID3D11Device* mDevice;
	Camera mCamera;
	aiScene* mScene;
	std::string mPath;

	Buffer mBVHBuffer;
	Buffer mIndexBuffer;
	Buffer mVertexBuffer;
	Buffer mTriangleProperties;

	uni::SamplerState mSampler;
	uni::Buffer mMaterialPropertyBuffer;	
	Texture mDiffuse;
	Texture mMetallicRoughness;
	Texture mNormal;

	// std::vector<MaterialProperty> mMaterialProperties;

	friend class Renderer; // TODO change the laziness
	friend class GUI;
};
