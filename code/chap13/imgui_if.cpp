#include "imgui_if.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dx12.h>
#pragma warning(pop)
#include "debug.h"
#include "init.h"
#include "util.h"
#include "../imgui/src/imgui_impl_win32.h"
#include "../imgui/src/imgui_impl_dx12.h"

using namespace Microsoft::WRL;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HRESULT ImguiIf::init(HWND hwnd)
{
	auto context = ImGui::CreateContext();

	if (context == nullptr)
	{
		ThrowIfFalse(false);
		return E_FAIL;
	}

	auto ret = ImGui_ImplWin32_Init(hwnd);

	if (!ret)
	{
		ThrowIfFalse(false);
		return E_FAIL;
	}

	m_descHeap = createDescriptorHeap();

	if (m_descHeap == nullptr)
	{
		ThrowIfFalse(false);
		return E_FAIL;
	}

	constexpr int32_t kNumFramesInFlight = 3;

	ret = ImGui_ImplDX12_Init(
		Resource::instance()->getDevice(),
		kNumFramesInFlight,
		Resource::instance()->getFrameBuffer(0)->GetDesc().Format,
		m_descHeap.Get(),
		m_descHeap.Get()->GetCPUDescriptorHandleForHeapStart(),
		m_descHeap.Get()->GetGPUDescriptorHandleForHeapStart()
	);

	if (!ret)
	{
		ThrowIfFailed(false);
		return E_FAIL;
	}

	// disable create ini file
	auto& io = ImGui::GetIO();
	io.IniFilename = nullptr;

	return S_OK;
}

void ImguiIf::newFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Rendering Test Menu");
	{
		static bool bFirst = true;

		if (bFirst)
		{
			bFirst = false;
			ImGui::SetWindowPos(kWindowPos);
			ImGui::SetWindowSize(kWindowSize, ImGuiCond_::ImGuiCond_FirstUseEver);
		}

		if (true /* test */)
		{
			{
				static bool blnChk = false;
				bool bUpdated = ImGui::Checkbox("CheckboxTest", &blnChk);
			}

			{
				bool bUpdated = false;

				static int32_t radio = 0;
				bUpdated |= ImGui::RadioButton("Radio 1", &radio, 0);
				ImGui::SameLine();
				bUpdated |= ImGui::RadioButton("Radio 2", &radio, 1);
				ImGui::SameLine();
				bUpdated |= ImGui::RadioButton("Radio 3", &radio, 2);

				if (bUpdated)
				{
					//DebugOutputFormatString("Radio button updated. (%d)\n", radio);
				}
			}

			{
				static int32_t nSlider = 0;
				bool bUpdated = ImGui::SliderInt("Int Slider", &nSlider, 0, 100);
			}

			{
				static float fSlider = 0.0f;
				bool bUpdated = ImGui::SliderFloat("Float Slider", &fSlider, 0.0f, 100.0f);
			}

			{
				static float col3[3] = { };
				bool bUpdated = ImGui::ColorPicker3("ColorPicker3", col3, ImGuiColorEditFlags_::ImGuiColorEditFlags_DisplayRGB);
			}

			{
				static float col4[4] = { };
				bool bUpdated = ImGui::ColorPicker4("ColorPicker4", col4, 
					ImGuiColorEditFlags_::ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_::ImGuiColorEditFlags_AlphaBar);
			}
		}
	}
	ImGui::End();
}

void ImguiIf::render(ID3D12GraphicsCommandList* list)
{
	ThrowIfFalse(list != nullptr);

	ImGui::Render();

	list->SetDescriptorHeaps(1, getDescHeap().GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
}

LRESULT ImguiIf::wndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ImguiIf::createDescriptorHeap() const
{
	ComPtr<ID3D12DescriptorHeap> descHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC desc = { };
	{
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = 1;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;
	}

	auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(descHeap.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	ret = descHeap.Get()->SetName(Util::getWideStringFromString("DescHeapImgui").c_str());
	ThrowIfFailed(ret);

	return descHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ImguiIf::getDescHeap()
{
	return m_descHeap;
}

