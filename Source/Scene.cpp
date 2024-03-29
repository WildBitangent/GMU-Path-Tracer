﻿#include "Scene.hpp"
#include <assimp/postprocess.h>
#include <stdexcept>
#include "spdlog/fmt/fmt.h"
#include <assimp/SceneCombiner.h>
#include <assimp/scene.h>
#include "assimp/pbrmaterial.h"
#include "lodepng/lodepng.h"
#include "avir/avir.h"
#include "avir/avir_float8_avx.h"
#include <thread>
#include "Constants.hpp"
#include <filesystem>
#include <fstream>
#include "CsvParser.hpp"

namespace fs = std::filesystem;
using namespace DirectX;

SceneParams SceneParams::instance = SceneParams();

void SceneParams::loadScenes()
{
	for (const auto& f : fs::recursive_directory_iterator(R"(Assets\Models\)"))
	{
		if (f.is_regular_file())
		{
			if (f.path().filename().extension().string() == ".gltf")
			{
				pathNames.emplace_back(f.path().string().substr(14)); // length of assets\models
				lights.emplace_back();
				cameraParams.emplace_back();

				auto paramsPath = f.path().string().substr(0, pathNames.back().length() + 9) + ".params";
				std::ifstream file(paramsPath);
				if (file.is_open())
				{
					CSVIterator params(file);

					for (size_t i = 0; i < sizeof(CameraParam) / 4; i++) // todo add some error checking for file integrity
						reinterpret_cast<float*>(&cameraParams.back())[i] = std::stof(params->operator[](i));

					auto getLight = [](const CSVRow& row) -> Light
					{
						Light light;

						for (size_t i = 0; i < sizeof(Light) / 4; i++)
							reinterpret_cast<float*>(&light)[i] = std::stof(row[i]);

						return light;
					};

					for (++params; params; ++params)
						lights.back().emplace_back(getLight(*params));
				}
				else
				{
					// default params
					cameraParams.back() = { {1.0, 3.0, 8.0}, 0, 270 };
					lights.back().emplace_back(Light{ {13.0f, 4.5f, 4.5f}, 100.0f, {80.0f, 80.0f, 40.0f}, 0.5f });
					lights.back().emplace_back(Light{ {0.0, 4.5, 2.0}, 100.0, {80.0, 80.0, 40.0}, 0.5 });
				}
			}
		}
	}

	for (const auto& p : pathNames)
		pathsReference.emplace_back(p.c_str());
	
	instance = *this;
}

size_t SceneParams::getSceneIndex(const std::string& name)
{
	for (int i = 0; i < pathNames.size(); ++i)
		if (name == pathNames[i])
			return i;

	throw std::runtime_error(fmt::format("Non existing scene {}", name));
}

Scene::Scene(ID3D11Device* device, const std::string& path)
	: mDevice(device)
	, mPath(path.substr(0, path.find_last_of('\\') + 1))
	, mSceneName(path.substr(14)) // offset of Assets\\Models\\ 
{	
	loadScene(path);
	
	std::thread worker(&Scene::createBVH, this);
	// createBVH();
	loadTextures();
	createSampler();
	createLights();

	const auto& cameraParams = SceneParams::instance.cameraParams[SceneParams::instance.getSceneIndex(mSceneName)];
	mCamera.getBuffer()->position = XMLoadFloat3(&cameraParams.position);
	mCamera.setRotation(cameraParams.pitch, cameraParams.yaw);

	worker.join();

	delete mScene; // won't be needed anymore
}

void Scene::update(float dt)
{
	mCamera.update(dt);
}

void Scene::loadScene(const std::string& path)
{
	Assimp::Importer importer;
	
	importer.ReadFile(path.c_str(), 
		aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices
		| aiProcess_SortByPType
		| aiProcess_GenSmoothNormals
		| aiProcess_FlipUVs
		| aiProcess_PreTransformVertices 
		//| aiProcess_FixInfacingNormals
	);

	mScene = importer.GetOrphanedScene();
}

void Scene::loadTextures()
{
	std::vector<MaterialProperty> materialProperties;

	for (size_t i = 0; i < mScene->mNumMaterials; i++)
	{
		MaterialProperty matProperty; 
		mScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, reinterpret_cast<aiColor4D&>(matProperty.color));
		mScene->mMaterials[i]->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, matProperty.metallic);
		mScene->mMaterials[i]->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, matProperty.roughness);

		aiString str;
		mScene->mMaterials[i]->Get(AI_MATKEY_GLTF_ALPHAMODE, str);
		if (str == aiString("BLEND"))
		{
			matProperty.materialType = MaterialProperty::GLASS;
			matProperty.refractIndex = 1.458; // glass refraction TODO remove
		}
		
		materialProperties.emplace_back(matProperty);
	}

	auto work = [&](aiTextureType type, MaterialProperty::Indices index, Texture& resource)
	{
		createTextures(loadSpecificTexture(type, materialProperties, index), resource);
	};
	
	std::thread diffWorker(work, aiTextureType_DIFFUSE, MaterialProperty::DIFFUSE, std::ref(mDiffuse));
	std::thread mtrWorker(work, aiTextureType_UNKNOWN, MaterialProperty::METALLICROUGHNESS, std::ref(mMetallicRoughness));
	std::thread normWorker(work, aiTextureType_NORMALS, MaterialProperty::NORMAL, std::ref(mNormal));
	
	// createTextures(loadSpecificTexture(aiTextureType_DIFFUSE, materialProperties, MaterialProperty::DIFFUSE), mDiffuse);
	// createTextures(loadSpecificTexture(aiTextureType_UNKNOWN, materialProperties, MaterialProperty::METALLICROUGHNESS), mMetallicRoughness);
	// createTextures(loadSpecificTexture(aiTextureType_NORMALS, materialProperties, MaterialProperty::NORMAL), mNormal);
	
	diffWorker.join();
	mtrWorker.join();
	normWorker.join();
	
	createPropertyBuffer(materialProperties);
}

void Scene::createBVH()
{
	// build BVH
	BVHWrapper bvh(mScene);

	// create buffers and upload data
	mBVHBuffer = createBuffer(mDevice, sizeof(BVHWrapper::BVHNode), bvh.mGPUTree);
	mIndexBuffer = createBuffer(mDevice, sizeof(BVHWrapper::Triangle), bvh.mIndices);
	mVertexBuffer = createBuffer(mDevice, sizeof(Vec3f), bvh.mVertices, DXGI_FORMAT_R32G32B32_FLOAT, {});
	mTriangleProperties = createBuffer(mDevice, sizeof(BVHWrapper::TriangleProperties), bvh.mTriangleProperties);
}

void Scene::createSampler()
{
	D3D11_SAMPLER_DESC samplerDescriptor = {};
	samplerDescriptor.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDescriptor.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescriptor.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescriptor.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDescriptor.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samplerDescriptor.MinLOD = 0;
	samplerDescriptor.MaxLOD = D3D11_FLOAT32_MAX;
	
	mDevice->CreateSamplerState(&samplerDescriptor, &mSampler);
}

void Scene::createPropertyBuffer(const std::vector<MaterialProperty>& data)
{
	D3D11_BUFFER_DESC materialPropDescriptor = {};
	materialPropDescriptor.Usage = D3D11_USAGE_DEFAULT;
	materialPropDescriptor.ByteWidth = sizeof(MaterialProperty) * mScene->mNumMaterials;
	materialPropDescriptor.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	materialPropDescriptor.CPUAccessFlags = 0;
	materialPropDescriptor.MiscFlags = 0;
	
	D3D11_SUBRESOURCE_DATA bufferData = {};
	bufferData.pSysMem = data.data();
	
	mDevice->CreateBuffer(&materialPropDescriptor, &bufferData, &mMaterialPropertyBuffer);
}

Scene::LoadedTextures Scene::loadSpecificTexture(aiTextureType type, std::vector<MaterialProperty>& properties, MaterialProperty::Indices index) const
{
	RawTextureData textures;
	std::set<unsigned> median;

	for (size_t i = 0; i < mScene->mNumMaterials; i++)
	{
		const auto& material = mScene->mMaterials[i];
		
		aiString path;
		material->GetTexture(type, 0, &path);
		
		if (path.length)
		{
			std::vector<unsigned char> texture;
			properties[i].textureIndices[index] = textures.size();

			unsigned width, height;
			lodepng::decode(texture, width, height, mPath + path.C_Str());
			textures.emplace_back(std::move(texture));

			median.emplace(textures.back().size());
			assert(width == height);
		}
	}

	unsigned medianVal = 0;

	if (!median.empty())
	{
		auto it = median.begin();
		std::advance(it, median.size() / 2);
		medianVal = static_cast<unsigned>(sqrt(*it / 4)); // gets width as median
	}

	return std::make_pair(std::move(textures), medianVal);
}

void Scene::createTextures(LoadedTextures rawTextures, Texture& resource)
{
	auto& textures = rawTextures.first;
	const auto dimension = rawTextures.second;

	if (textures.empty())
		return;
	
	D3D11_TEXTURE2D_DESC textureDescriptor = {};
	textureDescriptor.Width = dimension;
	textureDescriptor.Height = dimension;
	textureDescriptor.MipLevels = 1;
	textureDescriptor.ArraySize = textures.size();
	textureDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDescriptor.SampleDesc.Count = 1;
	textureDescriptor.SampleDesc.Quality = 0;
	textureDescriptor.Usage = D3D11_USAGE_DEFAULT;
	textureDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDescriptor.CPUAccessFlags = 0;
	textureDescriptor.MiscFlags = 0;

	std::vector<D3D11_SUBRESOURCE_DATA> initData;
	avir::CImageResizer<avir::fpclass_float8_dil> textureResizer(8);
	const auto textureSize = dimension * dimension * 4;

	for (auto& texture : textures)
	{
		if (texture.size() != textureSize)
		{
			std::vector<unsigned char> newTexture(textureSize);
			const auto oldDimension = static_cast<unsigned>(sqrt(texture.size() >> 2));
			
			textureResizer.resizeImage(
				texture.data(), oldDimension, oldDimension, 0, 
				newTexture.data(), dimension, dimension, 4, 0
			);
			
			texture.swap(newTexture);
		}
		
		D3D11_SUBRESOURCE_DATA textureData;
		textureData.pSysMem = texture.data();
		textureData.SysMemPitch = dimension * 4;
		textureData.SysMemSlicePitch = textureSize;
		
		initData.emplace_back(textureData);
	}
	
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDescriptor = {};
	srvDescriptor.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDescriptor.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDescriptor.Texture2DArray.ArraySize = textures.size();
	srvDescriptor.Texture2DArray.MipLevels = 1;
	
	mDevice->CreateTexture2D(&textureDescriptor, initData.data(), &resource.texture);
	mDevice->CreateShaderResourceView(resource.texture, &srvDescriptor, &resource.srv);
}

void Scene::createLights()
{
	D3D11_BUFFER_DESC lightDescriptor = {};
	lightDescriptor.Usage = D3D11_USAGE_DEFAULT;
	lightDescriptor.ByteWidth = MAX_LIGHTS * sizeof(Light);
	lightDescriptor.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	lightDescriptor.CPUAccessFlags = 0;
	lightDescriptor.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	lightDescriptor.StructureByteStride = sizeof(Light);

	const auto& lights = SceneParams::instance.lights[SceneParams::instance.getSceneIndex(mSceneName)];
	for (size_t i = 0; i < lights.size(); ++i)
		mLights[i] = lights[i];
	
	D3D11_SUBRESOURCE_DATA dataInit = {};
	dataInit.pSysMem = mLights.data();

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescriptor = {};
	SRVDescriptor.Format = DXGI_FORMAT_UNKNOWN;
	SRVDescriptor.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDescriptor.Buffer.NumElements = MAX_LIGHTS;

	mDevice->CreateBuffer(&lightDescriptor, &dataInit, &mLightBuffer.buffer);
	mDevice->CreateShaderResourceView(mLightBuffer.buffer, &SRVDescriptor, &mLightBuffer.srv);
}
