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
#include "bloom.h"
#include "floor.h"
#include "graph.h"
#include "observer.h"
#include "imgui_if.h"
#include "pmd_actor.h"
#include "pera.h"
#include "shadow.h"
#include "timestamp.h"
#include "toolkit.h"

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

class BaseResource
{
public:
	enum class Type {
		kColor,
		kNormal,
		kLuminance,
	};

	HRESULT createResource(DXGI_FORMAT format);
	HRESULT clearBaseRenderTargets(ID3D12GraphicsCommandList* list) const;
	HRESULT buildBarrier(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter) const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getRtvHeap() const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getSrvHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE getRtvCpuDescHandle(Type type) const;
	D3D12_GPU_DESCRIPTOR_HANDLE getSrvGpuDescHandle(Type type) const;

private:
	const std::array<float[4], 3> kClearColor = {
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 3> m_baseResources = { }; // MRT (0=color, 1=normal, 2=luminance)
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_baseRtvHeap = nullptr; // RT views (0:color, 1:normal, 2=luminance)
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_baseSrvHeap = nullptr; // SR views (0:color, 1:normal, 2=luminance)
};

class Render : public Observer
{
public:
	static Toolkit& toolkitInsntace() { return s_toolkit; }

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
	HRESULT createPostView();
	HRESULT updateMvpMatrix(bool animationReversed);
	HRESULT clearDepthRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsvH);
	HRESULT preProcessForOffscreenRendering(ID3D12GraphicsCommandList* list);

	static Toolkit s_toolkit;

	bool m_bAnimationEnabled = true;
	bool m_bAnimationReversed = false;
	bool m_bAutoMoveEyePos = false;
	bool m_bAutoMoveLightPos = false;
	DirectX::XMFLOAT3 m_eyePos = DirectX::XMFLOAT3(0.0f, 13.0f, -20.0f);
	DirectX::XMFLOAT3 m_focusPos = DirectX::XMFLOAT3(0.0f, m_eyePos.y, 0.0f);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_depthSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightDepthDsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightDepthSrvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_lightDepthResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneMatrixResource = nullptr;
	SceneMatrix* m_sceneMatrix = nullptr;
	DirectX::XMFLOAT3 m_parallelLightVec = { };
	BaseResource m_baseResource;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_postResource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_postRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_postSrvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	std::vector<PmdActor> m_pmdActors;

	Pera m_pera;
	Floor m_floor;
	Bloom m_bloom;
	Shadow m_shadow;
	RenderGraph m_graph;
	ImguiIf m_imguif;

	TimeStamp m_timeStamp;
};
