#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class DoF {
public:
	HRESULT init(UINT64 width, UINT height);
	HRESULT clearWorkRenderTarget(ID3D12GraphicsCommandList* list);
	HRESULT render(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle, ID3D12DescriptorHeap* pDepthSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getWorkDescSrvHeap() const;
	D3D12_GPU_DESCRIPTOR_HANDLE getWorkResourceSrcHandle() const;

private:
    static constexpr LPCWSTR kVsFile = L"dofVertex.hlsl";
    static constexpr LPCWSTR kPsFile = L"dofPixel.hlsl";

	enum class Pass {
		kDof,
		kCopy,
		// increment kNumPass if you add a new field
	};
	static constexpr size_t kNumPass = 2;
	static constexpr std::array<LPCSTR, kNumPass> kVsEntrypoints = { "main", "main" };
	static constexpr std::array<LPCSTR, kNumPass> kPsEntrypoints = { "main", "copyTex" };

    enum class SrvSlot {
        kBaseColor,
		kShrinkColor,
		kDepth,
    };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();
	HRESULT renderShrink(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle);
	HRESULT renderDof(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle, ID3D12DescriptorHeap* pDepthSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle);

	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, kNumPass> m_vsBlobs = { };
	std::array<Microsoft::WRL::ComPtr<ID3DBlob>, kNumPass> m_psBlobs = { };
	std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kNumPass> m_pipelineStates = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };

	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr; // to store shrink
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescSrvHeap = nullptr;
};
