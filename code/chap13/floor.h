#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Floor
{
public:
	HRESULT init();
	void setWorldMatrix();
	HRESULT renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap);
	HRESULT render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthLightSrvHeap);

private:
	const DirectX::XMMATRIX kDefaultTransMat = DirectX::XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	const DirectX::XMMATRIX kDefaultScaleMat = DirectX::XMMatrixScaling(20.0f, 1.0f, 20.0f);
	static constexpr LPCWSTR kVsFile = L"floorVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"floorPixel.hlsl";
	static constexpr LPCSTR kVsBasicEntryPoint = "basicVs";
	static constexpr LPCSTR kPsBasicEntryPoint = "basicWithShadowMapPs";
	static constexpr LPCSTR kVsShadowEntryPoint = "shadowVs";
	static constexpr D3D12_PRIMITIVE_TOPOLOGY kPrimTopology = D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	static constexpr size_t kNumOfVertices = 6;

	HRESULT loadShaders();
	HRESULT createVertexResource();
	HRESULT createTransformResource();
	HRESULT createRootSignature();
	HRESULT createGraphicsPipeline();
	void setInputAssembler(ID3D12GraphicsCommandList* list) const;
	void setRasterizer(ID3D12GraphicsCommandList* list) const;

	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_shadowVs = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_transDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_shadowPipelineState = nullptr;
	DirectX::XMMATRIX* m_pWorldMatrix = nullptr; // needs to be aligned 16 bytes
};
