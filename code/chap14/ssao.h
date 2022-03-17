#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Ssao
{
public:
	enum class TargetResource {
		kDstRt,
		kSrcDepth,
		kSrcNormal,
	};

	HRESULT init(UINT64 width, UINT64 height);
	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list); // TODO: imple
	void setResource(TargetResource target, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	HRESULT render(ID3D12GraphicsCommandList* list);

private:
	static constexpr LPCWSTR kVsFile = L"ssaoVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"ssaoPixel.hlsl";
	static constexpr LPCSTR kVsEntryPoint = "main";
	static constexpr LPCSTR kPsEntryPoint = "main";
	static constexpr FLOAT kClearColor[4] = { 0, 0, 0, 0 };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();
	void setupRenderTargetView();
	void setupShaderResourceView();
	HRESULT renderSsao(ID3D12GraphicsCommandList* list);
	HRESULT renderToTarget(ID3D12GraphicsCommandList* list);

	Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapRtv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapSrv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_dstResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcDepthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcNormalResource = nullptr;
};
