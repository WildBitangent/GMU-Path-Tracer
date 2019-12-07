#include "Window.hpp"
#include "Renderer.hpp"
#include "Input.hpp"

#include <stdexcept>
#include <cassert>
#include <chrono>

using namespace std::chrono_literals;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window(HINSTANCE hInstance, Resolution res, bool windowed, const std::string& name)
{
	const auto className = "PGR Projekt";

	RECT rect = {0, 0, res.first, res.second };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = wndCallback;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 2);
	wc.lpszClassName = className;
	wc.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

	if (!RegisterClassEx(&wc))
		throw std::runtime_error("Error registering class");
	
	mHwnd = CreateWindowEx(
		NULL,
		className,
		name.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);
	
	if (!mHwnd)
		throw std::runtime_error("Error creating window");

	ShowWindow(mHwnd, true);
	UpdateWindow(mHwnd);
}

void Window::setRenderer(Renderer* renderer)
{
	mRenderer = renderer;
}

void Window::loop()
{
	assert(mRenderer != nullptr);
	
	using clock = std::chrono::high_resolution_clock;
	
	auto timeStart = clock::now();
	
	for (MSG msg;;)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg); 
			DispatchMessage(&msg);
		}
		else 
		{
			auto currentTime = clock::now();
			auto deltaTime = currentTime - timeStart;
			timeStart = currentTime;

			auto count = std::chrono::duration_cast<std::chrono::nanoseconds>(deltaTime).count();
			
			mRenderer->update(count / 1'000'000'000.f);
			mRenderer->draw();
		}
	}
}

HWND Window::getHwnd() const
{
	return mHwnd;
}

LRESULT Window::wndCallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_GETMINMAXINFO:
	{
	    DefWindowProc(hwnd, msg, wParam, lParam);
	    MINMAXINFO* pmmi = (MINMAXINFO*)lParam;
	    pmmi->ptMaxTrackSize.x = 15360; // set to 16K
	    pmmi->ptMaxTrackSize.y = 8640;
	    return 0;
	}
	}

	if (Input::getInstance().update(hwnd, msg, wParam, lParam))
		return 0;

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return 0;
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
