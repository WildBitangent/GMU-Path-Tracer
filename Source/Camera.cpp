#include "Camera.hpp"
#include "Constants.hpp"
#include "windows.h"
#include "Input.hpp"

using namespace DirectX;

Camera::Camera()
{
	updateResolution(WIDTH, HEIGHT);
}

void Camera::updateResolution(size_t width, size_t height)
{
	const auto theta = 60 * 3.14f / 180; // 60
    const auto aspect = width / static_cast<float>(height);

    mHalfHeight = tanf(theta / 2.f);
    mHalfWidth = aspect * mHalfHeight;

	mCBuffer.pixelSize = XMFLOAT2(1.f / width, 1.f / height);
	mCBuffer.iterationCounter = -1;
}

void Camera::update(float dt)
{	
	// mouse handling
	const auto delta = Input::getInstance().getMouseDelta();
	mYaw += delta.x;
	mPitch -= delta.y;
	
    if (mPitch > 89.0f) mPitch = 89.0f;
    if (mPitch < -89.0f) mPitch = -89.0f;

	mFront = {
		cosf(XMConvertToRadians(mYaw)) * cosf(XMConvertToRadians(mPitch)),
		sinf(XMConvertToRadians(mPitch)),
		sinf(XMConvertToRadians(mYaw)) * cosf(XMConvertToRadians(mPitch)),
	};

	mFront = XMVector3Normalize(mFront);
	mLeft = XMVector3Normalize(XMVector3Cross({0, 1, 0}, mFront));
	mUp = XMVector3Normalize(XMVector3Cross(mFront, mLeft));

	
	// keyboard handling
	constexpr auto speed = 5.f;
	const auto velocity = speed * dt;
	
    if (Input::getInstance().keyActive('W'))
        mCBuffer.position += mFront * velocity;
    if (Input::getInstance().keyActive('S'))
        mCBuffer.position -= mFront * velocity;
    if (Input::getInstance().keyActive('A'))
        mCBuffer.position += mLeft * velocity;
    if (Input::getInstance().keyActive('D'))
        mCBuffer.position -= mLeft * velocity;
	

	// get view matrix and make uniforms
	auto view = XMMatrixTranspose(XMMatrixLookAtRH(mCBuffer.position, mFront + mCBuffer.position, mUp));
	
    const auto left = view.r[0];
    const auto up = view.r[1];
    const auto w = view.r[2];

	mCBuffer.upperLeftCorner = -mHalfWidth * left + mHalfHeight * up - w;
	mCBuffer.horizontal = 2 * mHalfWidth * left;
	mCBuffer.vertical = 2 * mHalfHeight * up;
	mCBuffer.iterationCounter++;
	
	if (delta.x != 0.f || delta.y != 0.f ||	Input::getInstance().anyActive())
		mCBuffer.iterationCounter = 0;

	const auto rrr = []() { return rand() / static_cast<float>(RAND_MAX); };
	
	mCBuffer.randomSeed = { rrr(), rrr() };
}

void Camera::setRotation(float pitch, float yaw)
{
	mPitch = pitch;
	mYaw = yaw;
}

Camera::CameraBuffer* Camera::getBuffer()
{
	return &mCBuffer;
}
