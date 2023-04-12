/*
* TODO:
* - glass
* - blur
*/

#pragma comment(lib, "D3D11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "D3DCompiler")

#include <iostream>
#include <string>
#include <fstream>
#include <cmath>
#include <chrono>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#include <glm\glm.hpp>
#include <glm\gtc\matrix_transform.hpp>

#define WND_WIDTH 1280
#define WND_HEIGHT 960

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

#include "input.h"
#include "window.h"
#include "scene.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
	Window window("Raytracer", WND_WIDTH, WND_HEIGHT);
	auto context = ImGui::CreateContext();
	ImGui::SetCurrentContext(context);
	ImGui_ImplWin32_Init(window.GetHandle());
	ImGui_ImplDX11_Init(window.GetDevice(), window.GetContext());

	Scene scene(&window, 0);
	scene.Load("scene.scene");

	float delta_time = 0;
	while (1) {
		auto frame_start = std::chrono::high_resolution_clock::now();
		window.PullMessages();
		window.GetInput()->Update();
		window.Clear(0.2, 0.4, 0.6);
		scene.UpdateCamera(delta_time);
		scene.UpdateCameraBuffer();
		window.Render();
		// ImGui
		ImGui_ImplWin32_NewFrame();
		ImGui_ImplDX11_NewFrame();
		ImGui::NewFrame();
		if (ImGui::Button("Save")) {
			scene.Save("scene.scene");
		}
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		window.Present();
		auto frame_end = std::chrono::high_resolution_clock::now();
		delta_time = std::chrono::duration<float, std::milli>(frame_end - frame_start).count();
	}
	return 0;
}