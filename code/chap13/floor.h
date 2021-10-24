#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Floor
{
public:
	HRESULT init();
	HRESULT renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap);
	HRESULT render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthLightSrvHeap);
	HRESULT renderAxis(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap);

private:
	struct VsType
	{
		enum Type
		{
			kBasic,
			kShadow,
			kAxis,
			kEnd,
		};
	};

	struct PsType
	{
		enum Type
		{
			kBasic,
			kAxis,
			kEnd,
		};
	};

	struct VbType
	{
		enum Type
		{
			kMesh,
			kAxis,
			kEnd,
		};
	};

	struct PipelineType
	{
		enum Type
		{
			kMesh,
			kShadow,
			kAxis,
			kEnd,
		};
	};

	struct TransMatrix
	{
		DirectX::XMMATRIX meshMatrix = { };
		DirectX::XMMATRIX axisMatrix = { };
	};

	const DirectX::XMMATRIX kDefaultTransMat = DirectX::XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	const DirectX::XMMATRIX kDefaultScaleMat = DirectX::XMMatrixScaling(20.0f, 1.0f, 20.0f);
	static constexpr LPCWSTR kVsFile = L"floorVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"floorPixel.hlsl";
	static constexpr std::array<LPCSTR, VsType::kEnd> kVsEntryPoints = { "basicVs", "shadowVs", "axisVs" };
	static constexpr std::array<LPCSTR, PsType::kEnd> kPsEntryPoints = { "basicWithShadowMapPs", "axisPs" };
	static constexpr D3D12_PRIMITIVE_TOPOLOGY kPrimTopology = D3D12_PRIMITIVE_TOPOLOGY::D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	void initMatrix();
	HRESULT loadShaders();
	HRESULT createVertexResource();
	HRESULT createTransformResource();
	HRESULT createRootSignature();
	HRESULT createGraphicsPipeline();
	void setInputAssembler(ID3D12GraphicsCommandList* list) const;
	void setRasterizer(ID3D12GraphicsCommandList* list, int32_t width, int32_t height) const;

	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, VsType::kEnd> m_vsArray = { };
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, PsType::kEnd> m_psArray = { };
	std::array<D3D12_VERTEX_BUFFER_VIEW, VbType::kEnd> m_vbViews = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, VbType::kEnd> m_vertResources = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_transResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_transDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, PipelineType::kEnd> m_pipelineStates = { };
	TransMatrix* m_pTransMatrix = nullptr; // needs to be aligned 16 bytes
};
