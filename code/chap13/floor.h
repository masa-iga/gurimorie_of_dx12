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
	static constexpr D3D12_PRIMITIVE_TOPOLOGY kPrimTopology = D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	static constexpr size_t kNumOfVertices = 6;

	HRESULT createVertexResource();
	void setInputAssembler(ID3D12GraphicsCommandList* list) const;
	void setRasterizer(ID3D12GraphicsCommandList* list) const;

	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertResource = { };
};
