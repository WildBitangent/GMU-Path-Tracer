#include "GUI.hpp"
#include "ImGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "ImGUI/imgui_impl_win32.h"
#include "Renderer.hpp"

GUI::GUI(Renderer& renderer)
	: mRenderer(renderer)
{
	IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
	
	// Setup Dear ImGui style
    ImGui::StyleColorsDark();
}

GUI::~GUI()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void GUI::init(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);
}

void GUI::update()
{
	ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    {
		ImGui::Begin("PGR Path Tracer");
		
		
        ImGui::Text("Current Paths %.3f GP", mRenderer.mScene.mCamera.getBuffer()->iterationCounter * 0.002);
        ImGui::Text("Iteration count %d", mRenderer.mScene.mCamera.getBuffer()->iterationCounter);
        ImGui::Text("Average %.3f ms/iteration (%.1f MP/s)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate * 2);

		{
			const char* items[] = { "HD 1280 x 720", "FHD 1920 x 1080", "QHD 2560 x 1440", "4K 3840 x 2160", "8K 7680 x 4320", "16K 15360 x 8640" };
			Renderer::Resolution resolutions[] = {
				{1280, 720},
				{1920, 1080},
				{2560, 1440},
				{3840, 2160},
				{7680, 4320},
				{15360, 8640},
			};

			if (ImGui::Combo("Resolution", &mPickedResolution, items, IM_ARRAYSIZE(items)))
				mRenderer.initResize(resolutions[mPickedResolution]);
		}

		{
			const char* scenes[] = { // todo automatic finding of models
				"testScene\\scene.gltf",
				"helmet\\scene.gltf",
				"bunny_glass\\bunny_glass_closed.gltf",
				"bunny_glass\\scene.gltf",
				"box\\box.gltf",
				"r3pu\\scene.gltf",
			};

			struct
			{
				DirectX::XMVECTOR pos;
				float pitch;
				float yaw;
			} const cameraParams[] = {
				{ {-2.5, 2.2, 11.0}, -9, 312 },
				{ {-2.6, 3.3, 1.3}, -23, 326 },
				{ {-2.8, 5.4, 4.2}, -36, 302 },
				{ {1.0, 3.0, 8.0}, 0, 270 },
				{ {-9.2, 0.4, -6.3}, 2, 376 },
				{ {8.6, -3.8, 17.8}, -2, 247 },
			};
			
			if (ImGui::Combo("Scene", &mPickedScene, scenes, IM_ARRAYSIZE(scenes)))
			{
				const auto& params = cameraParams[mPickedScene];
				mRenderer.initScene(scenes[mPickedScene]);
				mRenderer.mScene.mCamera.getBuffer()->position = params.pos;
				mRenderer.mScene.mCamera.setRotation(params.pitch, params.yaw);
			}
		}
		
        ImGui::End();
    }
}

void GUI::render() const
{
    ImGui::Render();
	
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}