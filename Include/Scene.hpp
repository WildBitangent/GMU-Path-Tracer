﻿#pragma once
#include "Camera.hpp"
#include "BVHWrapper.hpp"
#include <assimp/Importer.hpp>
#include <d3d11.h>
#include "UniqueDX11.hpp"
#include "Util.hpp"
#include <assimp/material.h>
#include <mutex>
#include "Constants.hpp"
#include <array>

struct Light
{
	DirectX::XMFLOAT3 position;
	float falloff;
	DirectX::XMFLOAT3 emission;
	float radius;
};

struct SceneParams
{
	SceneParams() { instance.loadScenes(); }
	
	void loadScenes();
	size_t getSceneIndex(const std::string& name);
	
	struct CameraParam
	{
		DirectX::XMFLOAT3 position;
		float pitch;
		float yaw;
	};
	
	std::vector<std::string> pathNames;
	std::vector<const char*> pathsReference;
	std::vector<std::vector<Light>> lights;
	std::vector<CameraParam> cameraParams;

	static SceneParams instance;
};

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
	void createLights();
	
private:	
	ID3D11Device* mDevice;
	Camera mCamera;
	aiScene* mScene;
	std::string mSceneName;
	std::string mPath;

	Buffer mBVHBuffer;
	Buffer mIndexBuffer;
	Buffer mVertexBuffer;
	Buffer mTriangleProperties;
	Buffer mLightBuffer;

	uni::SamplerState mSampler;
	uni::Buffer mMaterialPropertyBuffer;	
	Texture mDiffuse;
	Texture mMetallicRoughness;
	Texture mNormal;
	
	std::array<Light, MAX_LIGHTS> mLights;

	friend class Renderer; // TODO change the laziness
	friend class GUI;
};
