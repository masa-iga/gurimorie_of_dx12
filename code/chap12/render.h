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
#include "pera.h"

struct SceneMatrix
{
	DirectX::XMMATRIX view;
	DirectX::XMMATRIX proj;
	DirectX::XMFLOAT3 eye;
};

class Render {
public:
	HRESULT init();
	HRESULT update();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();
	void toggleAnimationEnable();
	void toggleAnimationReverse();

private:
	HRESULT loadImage();
	HRESULT createTextureBuffer();
	HRESULT createTextureBuffer2();
	HRESULT createSceneMatrixBuffer();
	HRESULT createViews();
	HRESULT createPeraView();
	HRESULT updateMvpMatrix();
	HRESULT preRenderToPeraBuffer();
	HRESULT postRenderToPeraBuffer();

	bool m_bAnimationEnabled = true;
	bool m_bAnimationReversed = false;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_texResource = nullptr;
	DirectX::TexMetadata m_metadata = { };
	DirectX::ScratchImage m_scratchImage = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneMatrixResource = nullptr;
	SceneMatrix* m_sceneMatrix = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	std::vector<PmdActor> m_pmdActors;

	Pera m_pera;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_peraResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraSrvHeap = nullptr;
};
