#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Bloom
{
public:
	HRESULT init(UINT64 width, UINT height);
	HRESULT render(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap *pSrcTexDescHeap);

private:
	static constexpr LPCWSTR kVsFile = L"bloomVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"bloomPixel.hlsl";

	enum class kType {
		kMain,
		kBlur,
		// increment kNumOfType if you are going to add a new field
	};
	static constexpr size_t kNumOfType = 2;
	static constexpr std::array<LPCSTR, kNumOfType> kVsEntryPoints = { "main", "main" };
	static constexpr std::array<LPCSTR, kNumOfType> kPsEntryPoints = { "main", "blurPs" };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();

	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, kNumOfType> m_vsBlobs = { };
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, kNumOfType> m_psBlobs = { };
	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapRtv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapSrv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = { };
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12RootSignature>, kNumOfType> m_rootSignatures = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kNumOfType> m_pipelineStates = { };
};
