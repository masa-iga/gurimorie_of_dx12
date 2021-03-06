#pragma once
#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <vector>
#include "pmd_reader.h"

struct SceneMatrix
{
	DirectX::XMMATRIX world;
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMFLOAT3 eye;
};

class Render {
public:
	HRESULT init();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();
	void toggleAnimationEnable();
	void toggleAnimationReverse();

private:
	HRESULT loadShaders();
	HRESULT loadImage();
	HRESULT createPipelineState();
	HRESULT createTextureBuffer();
	HRESULT createTextureBuffer2();
	HRESULT createSceneMatrixBuffer();
	HRESULT createViews();
	HRESULT updateMatrix();

	bool m_bAnimationEnabled = true;
	bool m_bAnimationReversed = false;

	ID3DBlob* m_vsBlob = nullptr;
	ID3DBlob* m_psBlob = nullptr;
	ID3D12RootSignature* m_rootSignature = nullptr;
	ID3D12PipelineState* m_pipelineState = nullptr;
	ID3D12DescriptorHeap* m_basicDescHeap = nullptr;
	ID3D12Resource* m_texResource = nullptr;
	DirectX::TexMetadata m_metadata = { };
	DirectX::ScratchImage m_scratchImage = { };
	ID3D12DescriptorHeap* m_dsvHeap = nullptr;
	ID3D12Resource* m_depthResource = nullptr;
	ID3D12Resource* m_mvpMatrixResource = nullptr;
	SceneMatrix* m_sceneMatrix = nullptr;

	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	PmdReader m_pmdReader;
};
