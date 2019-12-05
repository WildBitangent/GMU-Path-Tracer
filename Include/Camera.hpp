#pragma once
#include <DirectXMath.h>


class Camera // todo change
{
public:
	struct alignas(16) CameraBuffer
	{
		DirectX::XMVECTOR position = {1.f, 3.f, 8.0f};
		// DirectX::XMVECTOR position = {-16.f, 6.f, 1.0f};
		DirectX::XMVECTOR upperLeftCorner;
		DirectX::XMVECTOR horizontal;
		DirectX::XMVECTOR vertical;
		DirectX::XMFLOAT2 pixelSize;
		int32_t iterationCounter = -1;
		DirectX::XMFLOAT2 randomSeed;
	};
public:

	Camera();
	
	void updateResolution(size_t width, size_t height);
	void update(float dt);

	CameraBuffer* getBuffer();

private:
	CameraBuffer mCBuffer;
	DirectX::XMVECTOR mFront = {0.f, 0.f, 1.f}; 
	DirectX::XMVECTOR mUp = {0.0f, 1.0f, 0.0f};
	DirectX::XMVECTOR mLeft = {};

	float mHalfWidth;
	float mHalfHeight;

	float mPitch = {}; // todo maybe use radians as base?
	float mYaw = 270;
	// float mYaw = 356;
};
