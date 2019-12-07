#pragma once
#include <DirectXMath.h>
#include "Windows.h"
#include "Constants.hpp"
#include <utility>

class Input
{
	using Resolution = std::pair<unsigned, unsigned>;
public:
	static Input& getInstance();
	
	bool update(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	DirectX::XMFLOAT2 getMouseDelta();
	bool keyActive(const int key) const;
	bool anyActive() const;
	bool hasResized();
	Resolution getResolution() const;
	
private:
	bool mHasFocus = false;
	DirectX::XMFLOAT2 mMouseDelta = {};
	float mSensitivity = 0.15f;

	Resolution mResolution;
	bool mResized = false;
};
