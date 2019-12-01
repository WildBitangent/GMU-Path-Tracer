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
		POINT center = {WIDTH/2, HEIGHT/2};

		mMouseDelta = {
			(position.x - center.x) * mSensitivity,
			(position.y - center.y) * mSensitivity,
		};

		ClientToScreen(hwnd, &center);
		SetCursorPos(center.x, center.y);
	}
	else if (msg == WM_RBUTTONDOWN)
	{
		POINT center = {WIDTH/2, HEIGHT/2};
		ClientToScreen(hwnd, &center);
		SetCursorPos(center.x, center.y);
		ShowCursor(false);
	}
	else if(msg == WM_RBUTTONUP)
		ShowCursor(true);
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
