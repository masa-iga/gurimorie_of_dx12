#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Windows.h>
#include <d3d12.h>
#include <winnt.h>
#include <wrl.h>
#pragma warning(pop)
#include "../imgui/src/imgui.h"
#include "config.h"
#include "observer.h"

class ImguiIf : public Subject
{
public:
	HRESULT init(HWND hwnd);
	void teardown();
	void newFrame();
	void build();
	void render(ID3D12GraphicsCommandList* list);
	void setFps(float fps) { m_fps = fps; };
	void setEye(DirectX::XMFLOAT3 eye) { m_eye = eye; }
	void setFocus(DirectX::XMFLOAT3 focus) { m_focus = focus; }

	static LRESULT wndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	inline static const ImVec2 kWindowSize = { 350.0f, 300.0f };
	inline static const ImVec2 kWindowPos = { Config::kWindowWidth - kWindowSize.x, Config::kWindowHeight - kWindowSize.y };
	inline static const ImVec2 kTestWindowSize = { 350.0f, 700.0f };
	inline static const ImVec2 kTestWindowPos = { 0.0f, Config::kWindowHeight - kTestWindowSize.y };

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap() const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getDescHeap();
	void buildTestWindow();

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap = nullptr;
	float m_fps = 0.0f;
	DirectX::XMFLOAT3 m_eye = { };
	DirectX::XMFLOAT3 m_focus = { };
};
