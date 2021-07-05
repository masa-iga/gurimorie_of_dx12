#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Pera
{
public:
	HRESULT createView();
	HRESULT createTexture();
	HRESULT render();

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;
};
