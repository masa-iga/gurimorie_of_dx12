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
	HRESULT createVertexBufferView();
	HRESULT compileShaders();
	HRESULT createPipelineState();
	HRESULT render(ID3D12DescriptorHeap *pDescHeap);

private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_vs = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_ps = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_peraVertexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_peraVertexBufferView = { };

};
