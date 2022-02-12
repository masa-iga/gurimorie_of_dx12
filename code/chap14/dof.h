#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class DoF {
public:
	HRESULT init(UINT64 width, UINT height);
	HRESULT render(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle, ID3D12DescriptorHeap* pDepthSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle);
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getWorkDescSrvHeap() const;
	D3D12_GPU_DESCRIPTOR_HANDLE getWorkResourceSrcHandle() const;

private:
    static constexpr LPCWSTR kVsFile = L"dofVertex.hlsl";
    static constexpr LPCWSTR kPsFile = L"dofPixel.hlsl";
    static constexpr LPCSTR kVsEntrypoint = "main";
    static constexpr LPCSTR kPsEntrypoint = "main";

    enum class SrvSlot {
        kBaseColor = 0,
    };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();
	HRESULT renderShrink(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle);
	HRESULT renderDof(/* input: base, shrink, depth, output: rtv*/);

	Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };

	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr; // to store shrink
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescSrvHeap = nullptr;
};
