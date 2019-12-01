#pragma once
#include "Windows.h"
#include <utility>
#include <string>

class Renderer;

class Window
{
	using Resolution = std::pair<unsigned, unsigned>;
public:
	Window(HINSTANCE hInstance, Resolution res, bool windowed, const std::string& name);
	void setRenderer(Renderer* renderer);
	void loop();
	HWND getHwnd() const;
	
private:
	static LRESULT CALLBACK wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND mHwnd = nullptr;
	Renderer* mRenderer = nullptr;
	
};
