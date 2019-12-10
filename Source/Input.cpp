#include "Input.hpp"
#include "Windows.h"

Input& Input::getInstance()
{
	static Input instance;
	return instance;
}

bool Input::update(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!(mHasFocus = GetFocus() == hwnd))
		return false;

	if (msg == WM_MOUSEMOVE && (GetKeyState(VK_RBUTTON) & 0x100) != 0)
	{
		const auto position = MAKEPOINTS(lParam);
		POINT center = {mResolution.first / 2, mResolution.second / 2}; 

		mMouseDelta = {
			(position.x - center.x) * mSensitivity,
			(position.y - center.y) * mSensitivity,
		};

		ClientToScreen(hwnd, &center);
		SetCursorPos(center.x, center.y);
	}
	else if (msg == WM_RBUTTONDOWN)
	{
		POINT center = {mResolution.first / 2, mResolution.second / 2};
		ClientToScreen(hwnd, &center);
		SetCursorPos(center.x, center.y);
		ShowCursor(false);
	}
	else if (msg == WM_RBUTTONUP)
		ShowCursor(true);
	else if (msg == WM_SIZE)
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		
		mResolution = { rect.right - rect.left,  rect.bottom - rect.top };
		mResized = true;
	}
	else
		return false;

	return true;
}

DirectX::XMFLOAT2 Input::getMouseDelta()
{
	return mMouseDelta;
}

bool Input::keyActive(const int key) const
{
	return mHasFocus ? GetAsyncKeyState(key) : false;
}

bool Input::anyActive() const
{
	return mHasFocus ? GetAsyncKeyState('W') || GetAsyncKeyState('S') || GetAsyncKeyState('A') || GetAsyncKeyState('D') : false;
}

bool Input::hasResized()
{
	if (mResized)
	{
		mResized = false;
		return true;
	}

	return false;
}

Input::Resolution Input::getResolution() const
{
	return mResolution;
}
