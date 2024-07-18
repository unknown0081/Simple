#pragma once
#include <wtypes.h>
#include <vector>
#include <string>
#include "../../External/ImGui/imgui.h"
#include "../../External/ImGui/imgui_impl_win32.h"
#include "../../External/ImGui/imgui_impl_dx11.h"

//=======================================
#if _DEBUG
#define USE_IMGUI			0
#define SHOW_PERFORMANCE	0
#endif
//=======================================

//--------------------------------------------------------------
//  ImGuiManager
//--------------------------------------------------------------
class ImGuiManager
{
private:
	ImGuiManager() {}
	~ImGuiManager() {}

public:
	static ImGuiManager& Instance()
	{
		static ImGuiManager instance;
		return instance;
	}

	// 初期化
	void Initialize(HWND hwnd);
	// 終了化
	void Finalize();
	// 更新
	void Update();
	// 描画
	void Render();

	// DockSpace処理
	void DockSpace();
	// Console処理
	void Console();
	// DisplayPerformanceStats処理
	void DisplayPerformanceStats();

	// std::stringを使ったImGui::InputTextのラッパー関数
	bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags = 0);
	// std::stringを使ったImGui::InputTextのラッパー関数
	bool InputText(std::string label, std::string& str, ImGuiInputTextFlags flags = 0);
	// DirectX::XMFLOAT4を使ったImGui::ColorPicker4のラッパー関数
	void SimpleColor4(std::string label, DirectX::XMFLOAT4& color);
};