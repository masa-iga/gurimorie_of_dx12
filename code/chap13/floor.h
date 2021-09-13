#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Floor
{
public:
	HRESULT init();
	HRESULT renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap);
	HRESULT render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap);

private:
	static constexpr float kLength = 30.0f;
	static constexpr float kHeight = -5.0f;
	static constexpr LPCWSTR kVsFile = L"floorVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"floorPixel.hlsl";
	static constexpr D3D12_PRIMITIVE_TOPOLOGY kPrimTopology = D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	static constexpr size_t kNumOfVertices = 6;

	HRESULT loadShaders();
	HRESULT createVertexResource();
	HRESULT createRootSignature();
	HRESULT createGraphicsPipeline();
	void setInputAssembler(ID3D12GraphicsCommandList* list) const;
	void setRasterizer(ID3D12GraphicsCommandList* list) const;

	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertResource = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
};
