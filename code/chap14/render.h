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
#include "observer.h"
#include "imgui_if.h"
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

enum class MoveEye {
	kNone,
	kForward,
	kBackward,
	kRight,
	kLeft,
	kClockwise,
	kCounterClockwise,
	kUp,
	kDown,
};

class Render : public Observer
{
public:
	void onNotify(UiEvent uiEvent, bool flag);
	HRESULT init(HWND hwnd);
	void teardown();
	HRESULT update();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();
	void toggleAnimationEnable();
	void toggleAnimationReverse();
	void setFpsInImgui(float fps);
	void moveEye(MoveEye moveEye);

private:
	HRESULT createSceneMatrixBuffer();
	HRESULT createViews();
	HRESULT createPeraView();
	HRESULT updateMvpMatrix(bool animationReversed);
	HRESULT clearDepthRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsvH);
	HRESULT clearPeraRenderTargets(ID3D12GraphicsCommandList* list);
	HRESULT preProcessForOffscreenRendering(ID3D12GraphicsCommandList* list);
	HRESULT postProcessForOffScreenRendering(ID3D12GraphicsCommandList* list);

	bool m_bAnimationEnabled = true;
	bool m_bAnimationReversed = false;
	bool m_bAutoMoveEyePos = false;
	bool m_bAutoMoveLightPos = false;
	DirectX::XMFLOAT3 m_eyePos = DirectX::XMFLOAT3(0.0f, 13.0f, -20.0f);
	DirectX::XMFLOAT3 m_focusPos = DirectX::XMFLOAT3(0.0f, m_eyePos.y, 0.0f);

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
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> m_peraResources; // render target ([0] albedo, [1] normal)
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_peraSrvHeap = nullptr;

	Shadow m_shadow;
	ImguiIf m_imguif;

	TimeStamp m_timeStamp;
};
