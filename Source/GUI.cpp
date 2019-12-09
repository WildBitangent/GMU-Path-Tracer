﻿#include "GUI.hpp"
#include "ImGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "ImGUI/imgui_impl_win32.h"
#include "Renderer.hpp"
#include "spdlog/fmt/fmt.h"

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

		ImGui::Checkbox("Show Light Editor", &mShowEditor);
		
        ImGui::End();
    }

	if (mShowEditor) // todo refactor
	{
		ImGui::SetNextWindowSizeConstraints(ImVec2(700, 0),  ImVec2(700, FLT_MAX));
		ImGui::Begin("Light editor", &mShowEditor, ImGuiWindowFlags_NoCollapse);

		if (ImGui::Button("Add Light"))
		{
			ImGui::OpenPopup("Light Edit");
			mEditingLight = mRenderer.mScene.mCamera.getBuffer()->lightCount++;
			const auto& light = mRenderer.mScene.mLights[mEditingLight];
        	const auto power = std::max(light.emission.x, std::max(light.emission.y, light.emission.z));
			const DirectX::XMFLOAT3A color = { light.emission.x / power, light.emission.y / power, light.emission.z / power }; 

			// todo load light data
			mLightPos = light.position;
            mLightColor = color;
            mLightPower = power;
            mLightRadius = light.radius;

			
			mRenderer.mContext->UpdateSubresource(
				mRenderer.mScene.mLightBuffer.buffer, 
				0, nullptr, 
				mRenderer.mScene.mLights.data(), 
				0, 0
			);
			mRenderer.mScene.mCamera.getBuffer()->iterationCounter = -1;
		}

		ImGui::Columns(3, nullptr, false);
        for (size_t i = 0; i < mRenderer.mScene.mCamera.getBuffer()->lightCount; i++)
        {
			const auto& light = mRenderer.mScene.mLights[i];
        	const auto power = std::max(light.emission.x, std::max(light.emission.y, light.emission.z));
			DirectX::XMFLOAT3A color = { light.emission.x / power, light.emission.y / power, light.emission.z / power }; 
        	
            if (ImGui::GetColumnIndex() == 0)
                ImGui::Separator();
        	
            ImGui::Text("Light %d", i);
        	// ImGui::Text("Light color");
        	ImGui::SameLine();
			ImGui::ColorButton("Light Color", *(ImVec4*)&color, 0, ImVec2(18,18));

        	ImGui::Text("Position: %.2f  %.2f  %.2f", light.position.x, light.position.y, light.position.z);
        	ImGui::Text("Emission: %.2f  %.2f  %.2f", light.emission.x, light.emission.y, light.emission.z);
        	
            if (ImGui::Button(fmt::format("Edit {}", i).c_str(), ImVec2(-FLT_MIN, 0.0f)))
            {
				ImGui::OpenPopup("Light Edit");
	            mEditingLight = i;
				
				mLightPos = light.position;
            	mLightColor = color;
            	mLightPower = power;
            	mLightRadius = light.radius;
            }
        	
            ImGui::NextColumn();
        }
		
		bool dummy = true;
		ImGui::SetNextWindowSizeConstraints(ImVec2(320, -1),  ImVec2(320, -1));
        if (ImGui::BeginPopupModal("Light Edit", &dummy))
        {
			ImGui::Checkbox("Live updating", &mUpdating);
        	
        	auto& light = mRenderer.mScene.mLights[mEditingLight];
        	
        	ImGui::ColorPicker3("Light Color", (float*)&mLightColor, 0);
			
            ImGui::Separator();
        	ImGui::DragFloat3("Position", reinterpret_cast<float*>(&mLightPos), 0.1);
        	ImGui::DragFloat("Power", &mLightPower, 0.1);
        	ImGui::DragFloat("Radius", &mLightRadius, 0.1);

        	if (mUpdating)
        	{
				light.emission = { mLightColor.x * mLightPower, mLightColor.y * mLightPower, mLightColor.z * mLightPower };
        		light.position = mLightPos;
        		light.radius = mLightRadius;

        		mRenderer.mContext->UpdateSubresource(
					mRenderer.mScene.mLightBuffer.buffer, 
					0, nullptr, 
					mRenderer.mScene.mLights.data(), 
					0, 0
				);
        	}
        	
            ImGui::Separator();
            if (ImGui::Button("Update"))
            {
            	light.emission = { mLightColor.x * mLightPower, mLightColor.y * mLightPower, mLightColor.z * mLightPower };
        		light.position = mLightPos;
        		light.radius = mLightRadius;

            	mRenderer.mContext->UpdateSubresource(
					mRenderer.mScene.mLightBuffer.buffer, 
					0, nullptr, 
					mRenderer.mScene.mLights.data(), 
					0, 0
				);
				mRenderer.mScene.mCamera.getBuffer()->iterationCounter = -1;
            	
                ImGui::CloseCurrentPopup();
            }
        	ImGui::SameLine();
        	if (ImGui::Button("Delete"))
        	{
        		for (size_t i = mEditingLight; i < mRenderer.mScene.mCamera.getBuffer()->lightCount; i++)
					mRenderer.mScene.mLights[i] = mRenderer.mScene.mLights[i + 1];

        		mRenderer.mScene.mCamera.getBuffer()->lightCount--;
        		
				mRenderer.mContext->UpdateSubresource(
					mRenderer.mScene.mLightBuffer.buffer, 
					0, nullptr, 
					mRenderer.mScene.mLights.data(), 
					0, 0
				);
				mRenderer.mScene.mCamera.getBuffer()->iterationCounter = -1;

        		ImGui::CloseCurrentPopup();
        	}

            ImGui::EndPopup();
        }
        
		ImGui::End();
	}
}

void GUI::render() const
{
    ImGui::Render();
	
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}