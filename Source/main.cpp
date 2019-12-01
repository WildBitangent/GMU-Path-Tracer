#include "Window.hpp"
#include "Renderer.hpp"
#include "Constants.hpp"

#include <exception>
#include <iostream>
#include <d3d11.h>
#include <roapi.h>

int WINAPI CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	try
	{
		Window window(hInstance, { WIDTH, HEIGHT }, true, "PGR Projekt");
		Renderer renderer(window.getHwnd(), {WIDTH, HEIGHT});
		window.setRenderer(&renderer);
		window.loop();
	}
	catch (const std::exception& e)
	{
		OutputDebugString(e.what());
		return -1;
	}

	return 0;
}
