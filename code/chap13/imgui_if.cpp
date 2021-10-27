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

	return S_OK;
}

void ImguiIf::newFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Rendering Test Menu");
	ImGui::SetWindowPos(kWindowPos);
	ImGui::SetWindowSize(kWindowSize, ImGuiCond_::ImGuiCond_FirstUseEver);
	ImGui::End();
}

void ImguiIf::render(ID3D12GraphicsCommandList* list)
{
	ThrowIfFalse(list != nullptr);

	ImGui::Render();

	list->SetDescriptorHeaps(1, getDescHeap().GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
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

