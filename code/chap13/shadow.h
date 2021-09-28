#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
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
	HRESULT compileShaders();
	HRESULT createVertexBuffer();
	HRESULT createPipelineState();

	Microsoft::WRL::ComPtr<ID3DBlob> m_triangleVs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_trianglePs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_linePs = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_trianglePipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_linePipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vbTriangleResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vbLineResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbTriangleView = { };
	D3D12_VERTEX_BUFFER_VIEW m_vbLineView = { };
};

