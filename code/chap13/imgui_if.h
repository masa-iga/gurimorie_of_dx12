#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <d3d12.h>
#include <winnt.h>
#include <wrl.h>
#pragma warning(pop)
#include "../imgui/src/imgui.h"
#include "config.h"

class ImguiIf {
public:
	HRESULT init(HWND hwnd);
	void newFrame();
	void render(ID3D12GraphicsCommandList* list);

private:
	inline static const ImVec2 kWindowSize = { 500.0f, 400.f };
	inline static const ImVec2 kWindowPos = { 0.0f, Config::kWindowHeight - kWindowSize.y };

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap() const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getDescHeap();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap = nullptr;
};
