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
#include "floor.h"
#include "pmd_actor.h"
#include "pera.h"
#include "shadow.h"
#include "timestamp.h"

struct SceneMatrix
{
	DirectX::XMMATRIX view = { };
	DirectX::XMMATRIX proj = { };
	DirectX::XMMATRIX lightCamera = { };
	DirectX::XMMATRIX shadow = { };
	DirectX::XMFLOAT3 eye = { };
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
	HRESULT createSceneMatrixBuffer();
	HRESULT createViews();
	HRESULT createPeraView();
	HRESULT updateMvpMatrix();
	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtvH);
	HRESULT clearDepthRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsvH);
	HRESULT clearPeraRenderTarget(ID3D12GraphicsCommandList* list);
	HRESULT preRenderToPeraBuffer(ID3D12GraphicsCommandList* list);
	HRESULT postRenderToPeraBuffer(ID3D12GraphicsCommandList* list);

	bool m_bAnimationEnabled = true;
	bool m_bAnimationReversed = false;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_depthSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightDepthDsvHeap = nullptr; // TODO: bundle the heaps into one
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightDepthSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_lightDepthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneMatrixResource = nullptr;
	SceneMatrix* m_sceneMatrix = nullptr;
	DirectX::XMFLOAT3 m_parallelLightVec = { };

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	Floor m_floor;

	std::vector<PmdActor> m_pmdActors;

	Pera m_pera;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_peraResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraSrvHeap = nullptr;

	Shadow m_shadow;

	TimeStamp m_timeStamp;
};
