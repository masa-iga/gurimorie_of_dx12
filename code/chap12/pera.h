#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class Pera
{
public:
	HRESULT createResources();
	HRESULT compileShaders();
	HRESULT createPipelineState();
	HRESULT render(const D3D12_CPU_DESCRIPTOR_HANDLE *pRtvHeap, ID3D12DescriptorHeap *pSrvDescHeap);

private:
	HRESULT createVertexBufferResource();
	HRESULT createBokehResource();
	HRESULT createOffscreenResource();
	HRESULT createEffectBufferAndView();
	HRESULT renderBokeh(const D3D12_CPU_DESCRIPTOR_HANDLE *pRtvHeap, ID3D12DescriptorHeap *pSrvDescHeap);
	HRESULT renderEffect(const D3D12_CPU_DESCRIPTOR_HANDLE *pRtvHeap, ID3D12DescriptorHeap *pSrvDescHeap);

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature_bokeh = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature_effect = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState_bokehH = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState_bokehV = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState_effect = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps_bokehH = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps_bokehV = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps_effect = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_peraVertexBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_bokehParamBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_offscreenBuffer = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_effectTexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_peraVertexBufferView = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr; // Gaussian param
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_offscreenRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_offscreenSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_effectSrvHeap = nullptr;
};
