#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <d3d12.h>
#include <winnt.h>
#include <wrl.h>
#pragma warning(pop)

class ImguiIf {
public:
	HRESULT init(HWND hwnd);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getDescHeap();

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap() const;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap = nullptr;
};
