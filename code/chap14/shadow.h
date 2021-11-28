#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <array>
#include <d3d12.h>
#include <Windows.h>
#include <wrl.h>
#pragma warning(pop)

class Shadow {
public:
	HRESULT init();
	HRESULT render(ID3D12GraphicsCommandList* pCommandList, const D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHeap, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> texDescHeap);
	HRESULT render(ID3D12GraphicsCommandList* pCommandList, const D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHeap, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> texDescHeap, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);

private:
	enum class Type
	{
		kQuadR,
		kFrameLine,
		kEnd,
	};

	enum class VbType
	{
		kQuad,
		kFrameLine,
		kEnd,
	};

	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createPipelineState();

	Microsoft::WRL::ComPtr<ID3DBlob> m_commonVs = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, static_cast<size_t>(Type::kEnd)> m_psArray = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, static_cast<size_t>(Type::kEnd)> m_pipelineStates = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, static_cast<size_t>(VbType::kEnd)> m_vbResources = { };
	std::array<D3D12_VERTEX_BUFFER_VIEW, static_cast<size_t>(Type::kEnd)> m_vbViews = { };
};

