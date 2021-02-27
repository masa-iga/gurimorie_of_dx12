#pragma once
#include <Windows.h>
#include <d3d12.h>

class Render {
public:
	HRESULT init();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();

private:
	HRESULT loadShaders();
	HRESULT createPipelineState();
	HRESULT createVertexBuffer();

	ID3DBlob* m_vsBlob = nullptr;
	ID3DBlob* m_psBlob = nullptr;
	ID3D12RootSignature* m_rootSignature = nullptr;
	ID3D12PipelineState* m_pipelineState = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };

	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_fenceVal = 0;
};
