#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <array>
#include <Windows.h>
#include <d3d12.h>
#include <map>
#include <wrl.h>
#pragma warning(pop)

class Ssao
{
public:
	enum class TargetResource {
		kDstRt,
		kSrcDepth,
		kSrcNormal,
		kSrcSceneParam,
	};

	HRESULT init(UINT64 width, UINT64 height);
	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list);
	void setResource(TargetResource target, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	Microsoft::WRL::ComPtr<ID3D12Resource> getWorkResource() const { return m_workResource; }
	HRESULT render(ID3D12GraphicsCommandList* list);

private:
	static constexpr LPCWSTR kVsFile = L"ssaoVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"ssaoPixel.hlsl";
	static constexpr std::array<LPCSTR, 2> kVsEntryPoints = { "main", "main" };
	static constexpr std::array<LPCSTR, 2> kPsEntryPoints = { "ssao", "resolve" };
	static constexpr FLOAT kClearColor[4] = { 0, 0, 0, 0 };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();
	void setupRenderTargetView();
	void setupShaderResourceView();
	HRESULT renderSsao(ID3D12GraphicsCommandList* list);
	HRESULT renderToTarget(ID3D12GraphicsCommandList* list);

	std::map<LPCSTR, Microsoft::WRL::ComPtr<ID3DBlob>> m_vsBlobTable;
	std::map<LPCSTR, Microsoft::WRL::ComPtr<ID3DBlob>> m_psBlobTable;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapRtv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapCbvSrv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_dstResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcDepthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcNormalResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcSceneParamResource = nullptr;
};
