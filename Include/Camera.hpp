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
		
		DirectX::XMFLOAT2 randomSeed;
		int32_t iterationCounter = -1;	// TODO these shouldn't be in camera buffer
		uint32_t lightCount = 2;
		uint32_t sampleLights = false;
	};
public:

	Camera();
	
	void updateResolution(size_t width, size_t height);
	void update(float dt);
	void setRotation(float pitch, float yaw);
	
	CameraBuffer* getBuffer();

private:
	CameraBuffer mCBuffer;
	DirectX::XMVECTOR mFront = {0.f, 0.f, 1.f}; 
	DirectX::XMVECTOR mUp = {0.0f, 1.0f, 0.0f};
	DirectX::XMVECTOR mLeft = {};

	float mHalfWidth;
	float mHalfHeight;

	float mPitch = {};
	float mYaw = 270;

	bool moveHysteresis = false;
};
