#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <array>
#include <Windows.h>
#include <d3d12.h>
#include <dxcapi.h>
#include <map>
#include <wrl.h>
#pragma warning(pop)

#define USE_DXC_IN_SSAO (1)

class Ssao
{
public:
	enum class TargetResource {
		kDstRt,
		kSrcDepth,
		kSrcNormal,
		kSrcColor,
		kSrcSceneParam,
	};

	HRESULT init(UINT64 width, UINT64 height);
	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list);
	void setResource(TargetResource target, Microsoft::WRL::ComPtr<ID3D12Resource> resource);
	Microsoft::WRL::ComPtr<ID3D12Resource> getWorkResource() const { return m_workResource; }
	HRESULT render(ID3D12GraphicsCommandList* list);

private:
	enum class Type {
		kSsao,
		kResolve,
		kEnd,
	};

	static constexpr LPCWSTR kVsFile = L"ssaoVertex.hlsl";
	static constexpr LPCWSTR kPsFile = L"ssaoPixel.hlsl";
#if USE_DXC_IN_SSAO
	static constexpr std::array<LPCWSTR, static_cast<size_t>(Type::kEnd)> kVsEntryPointsDxc = { L"main", L"main" };
	static constexpr std::array<LPCWSTR, static_cast<size_t>(Type::kEnd)> kPsEntryPointsDxc = { L"ssao", L"resolve" };
#else
	static constexpr std::array<LPCSTR, static_cast<size_t>(Type::kEnd)> kVsEntryPoints = { "main", "main" };
	static constexpr std::array<LPCSTR, static_cast<size_t>(Type::kEnd)> kPsEntryPoints = { "ssao", "resolve" };
#endif // USE_DXC_IN_SSAO
	static constexpr FLOAT kClearColor[4] = { 0, 0, 0, 0 };

	HRESULT compileShaders();
	HRESULT createResource(UINT64 dstWidth, UINT dstHeight);
	HRESULT createRootSignature();
	HRESULT createPipelineState();
	void setupRenderTargetView();
	void setupShaderResourceView();
	HRESULT renderSsao(ID3D12GraphicsCommandList* list);
	HRESULT renderToTarget(ID3D12GraphicsCommandList* list);

#if USE_DXC_IN_SSAO
	std::map<LPCWSTR, Microsoft::WRL::ComPtr<IDxcBlob>> m_vsBlobTableDxc;
	std::map<LPCWSTR, Microsoft::WRL::ComPtr<IDxcBlob>> m_psBlobTableDxc;
#else
	std::map<LPCSTR, Microsoft::WRL::ComPtr<ID3DBlob>> m_vsBlobTable;
	std::map<LPCSTR, Microsoft::WRL::ComPtr<ID3DBlob>> m_psBlobTable;
#endif // USE_DXC_IN_SSAO
	Microsoft::WRL::ComPtr<ID3D12Resource> m_workResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapRtv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_workDescHeapCbvSrv = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	std::map<Type, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_pipelineStateTable;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_dstResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcDepthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcNormalResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcColorResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_srcSceneParamResource = nullptr;
};