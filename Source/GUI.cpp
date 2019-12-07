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
		ImGui::ShowDemoWindow();
		ImGui::Begin("PGR Path Tracer");
		

        ImGui::Text("Iteration count: %d", mRenderer.mScene.mCamera.getBuffer()->iterationCounter);

		
        //ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        //ImGui::Checkbox("Another Window", &show_another_window);

        //ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        // ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        //if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    counter++;
        //ImGui::SameLine();
        //ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/iteration (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

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
		
        ImGui::End();
    }

    // 3. Show another simple window.
    // if (show_another_window)
    // {
    //     ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
    //     ImGui::Text("Hello from another window!");
    //     if (ImGui::Button("Close Me"))
    //         show_another_window = false;
    //     ImGui::End();
    // }
}

void GUI::render() const
{
    ImGui::Render();
	
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}