#pragma once
#include <DirectXMath.h>
#include "Windows.h"
#include "Constants.hpp"

class Input
{
public:
	static Input& getInstance();
	
	bool update(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	DirectX::XMFLOAT2 getMouseDelta();
	bool keyActive(const int key) const;
	bool anyActive() const;
	
private:
	bool mHasFocus = false;
	DirectX::XMFLOAT2 mMouseDelta = {};
	float mSensitivity = 0.15f;
	
};
