#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <DirectXTex.h>
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#pragma warning(pop)
#include "pmd_actor.h"

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
	HRESULT update();
	HRESULT draw();
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

	Microsoft::WRL::ComPtr<ID3DBlob> m_vsBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> m_psBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_basicDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_texResource = nullptr;
	DirectX::TexMetadata m_metadata = { };
	DirectX::ScratchImage m_scratchImage = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_mvpMatrixResource = nullptr;
	SceneMatrix* m_sceneMatrix = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	std::vector<PmdActor> m_pmdActors;
};
