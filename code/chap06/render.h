#pragma once
#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <vector>

struct TexRgba
{
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
	uint8_t a = 0;
};

class Render {
public:
	HRESULT init();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();

private:
	HRESULT loadShaders();
	HRESULT loadImage();
	HRESULT createPipelineState();
	HRESULT createVertexBuffer();
	HRESULT createTextureBuffer();
	HRESULT createTextureBuffer2();
	HRESULT createViews();

	ID3DBlob* m_vsBlob = nullptr;
	ID3DBlob* m_psBlob = nullptr;
	ID3D12RootSignature* m_rootSignature = nullptr;
	ID3D12PipelineState* m_pipelineState = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };
	ID3D12Resource* m_texResource = nullptr;
	ID3D12DescriptorHeap* m_texDescHeap = nullptr;
	DirectX::TexMetadata m_metadata = { };
	DirectX::ScratchImage m_scratchImage = { };

	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_fenceVal = 0;
};
