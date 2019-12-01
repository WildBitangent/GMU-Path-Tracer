#include "BVHWrapper.hpp"
#include "Nvidia-SBVH/BVH.h"
#include "assimp/scene.h"
#include <stack>


BVHWrapper::BVHWrapper(const aiScene* scene)
	: mScene(scene)
{
	buildSBVH();
}

void BVHWrapper::buildSBVH()
{
	Array<GPUScene::Triangle> triangles;
	
	for (size_t i = 0; i < mScene->mNumMeshes; ++i)
	{
		auto& mesh = *mScene->mMeshes[i];

		// processing only triangles (means points and lines are not rendered)
		if (~mesh.mPrimitiveTypes & aiPrimitiveType_TRIANGLE) 
			continue;
		
		// insert vertices
		const auto offset = mVertices.getSize();
		mVertices.add(reinterpret_cast<Vec3f*>(mesh.mVertices), mesh.mNumVertices);

		// triangle properties
		for (size_t n = 0; n < mesh.mNumVertices; n++)
		{
			mTriangleProperties.emplace_back(TriangleProperties{
				{mesh.mNormals[n].x, mesh.mNormals[n].y, mesh.mNormals[n].z},
				(mesh.HasTextureCoords(0) ? *reinterpret_cast<DirectX::XMFLOAT2*>(&mesh.mTextureCoords[0][n]) : DirectX::XMFLOAT2{ 0, 0 }),
				mesh.mMaterialIndex
			});
		}

		// triangles (indices)		
		for (size_t n = 0; n < mesh.mNumFaces; ++n)
		{
			auto& f = mesh.mFaces[n];
			
			Vec3i triangle(f.mIndices[0] + offset, f.mIndices[1] + offset, f.mIndices[2] + offset);
			triangles.add(*reinterpret_cast<GPUScene::Triangle*>(&triangle));
		}
	}

	
    GPUScene scene = GPUScene(triangles.getSize(), mVertices.getSize(), triangles, mVertices);
	
   	const Platform defaultPlatform;
	const BVH::BuildParams defaultParams;
	BVH bvh(&scene, defaultPlatform, defaultParams);

	std::stack<std::pair<::BVHNode*, size_t>> stack{ { {bvh.getRoot(), 0} } };
	mGPUTree.resize(bvh.getNumNodes());

	for (size_t nodeIndex = 0; !stack.empty(); )
	{
		auto [root, currentIndex] = stack.top(); stack.pop();
		auto& aabb = root->m_bounds;
		auto& node = mGPUTree[currentIndex];
		node.min = { aabb.min().x, aabb.min().y, aabb.min().z };
		node.max = { aabb.max().x, aabb.max().y, aabb.max().z };
		node.isLeaf = false;
				
		if (root->isLeaf())
		{
			const auto leaf = reinterpret_cast<const LeafNode*>(root);
			const auto start = mIndices.size();

			node.leftIndex = start;
			node.rightIndex = start + leaf->m_hi - leaf->m_lo;
			node.isLeaf = true;

			// join vertex indices and triangle id (used for material)
			for (auto i = leaf->m_lo; i < leaf->m_hi; i++)
			{
				const uint32_t index = bvh.getTriIndices()[i];
				const auto& indices = bvh.getScene()->getTriangle(index).vertices;
				mIndices.emplace_back(Triangle{{indices.x, indices.y, indices.z}, mTriangleProperties[indices.x].materialID }); // TODO is matID right?
			}
		}
		else
		{
			nodeIndex += 2;
			stack.push({ root->getChildNode(1), nodeIndex });
			node.rightIndex = stack.top().second;
			stack.push({ root->getChildNode(0), nodeIndex - 1 });
			node.leftIndex = stack.top().second;
		}

		mGPUTree.emplace_back(node);
	}
}

