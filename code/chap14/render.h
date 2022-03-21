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
#include "config.h"
#include "dof.h"
#include "floor.h"
#include "graph.h"
#include "observer.h"
#include "imgui_if.h"
#include "pmd_actor.h"
#include "pera.h"
#include "shadow.h"
#include "ssao.h"
#include "timestamp.h"
#include "toolkit.h"

struct SceneParam
{
	DirectX::XMMATRIX view = { };
	DirectX::XMMATRIX proj = { };
	DirectX::XMMATRIX lightCamera = { };
	DirectX::XMMATRIX shadow = { };
	DirectX::XMFLOAT3 eye = { };
	float highLuminanceThreshold = 0.0f;
};

enum class MoveEye {
	kNone,
	kPosX,
	kPosY,
	kPosZ,
	kFocusX,
	kFocusY,
};

class OffScreenResource
{
public:
	enum class Type {
		kColor,
		kNormal,
		kLuminance,
		kPostSsao,
		kPostBloom,
		kPostDof,
		// Do not forget to increase kNumResource if you add a new field
	};
	static constexpr size_t kNumResource = 6;

	HRESULT createResource(DXGI_FORMAT format);
	HRESULT clearRenderTargets(ID3D12GraphicsCommandList* list) const;
	HRESULT buildBarrier(ID3D12GraphicsCommandList* list, Type type, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter) const;
	HRESULT buildBarrier(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter) const;
	Microsoft::WRL::ComPtr<ID3D12Resource> getResource(Type type) const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getRtvHeap() const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> getSrvHeap() const;
	D3D12_CPU_DESCRIPTOR_HANDLE getRtvCpuDescHandle(Type type) const;
	D3D12_GPU_DESCRIPTOR_HANDLE getSrvGpuDescHandle(Type type) const;

private:
	const std::array<float[4], kNumResource> kClearColor = {
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
	};

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kNumResource> m_resources = { };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;
};

class Render : public Observer
{
public:
	static Toolkit& toolkitInsntace() { return s_toolkit; }

	void onNotify(UiEvent uiEvent, const void* uiEventData);

	HRESULT init(HWND hwnd);
	void teardown();
	HRESULT update();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();
	void toggleAnimationEnable();
	void toggleAnimationReverse();
	void setFpsInImgui(float fps);
	void moveEye(MoveEye moveEye, float val = 0.0f);

private:
	HRESULT createSceneMatrixBuffer();
	HRESULT createViews();
	HRESULT updateMvpMatrix(bool animationReversed);
	void updateHighLuminanceThreshold(float val);
	HRESULT clearDepthRenderTargets(ID3D12GraphicsCommandList* list);
	void renderShadowPass(ID3D12GraphicsCommandList* list);
	void renderBasePass(ID3D12GraphicsCommandList* list);
	void renderPostPass(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE fbRtvHandle);
	void renderDebugPass(ID3D12GraphicsCommandList* list, const D3D12_CPU_DESCRIPTOR_HANDLE* pRtCpuDescHandle);
	void resolveResourceBarrier(ID3D12GraphicsCommandList* list, ID3D12Resource* backBufferResource);

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
	Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneParamResource = nullptr;
	SceneParam* m_sceneParam = nullptr;
	DirectX::XMFLOAT3 m_parallelLightVec = { };
	OffScreenResource m_offScreenResource;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence = nullptr;
	UINT64 m_fenceVal = 0;

	std::vector<PmdActor> m_pmdActors;

	Pera m_pera;
	Floor m_floor;
	Bloom m_bloom;
	DoF m_dof;
	Ssao m_ssao;
	Shadow m_shadow;
	RenderGraph m_graph;
	ImguiIf m_imguif;

	TimeStamp m_timeStamp;
};
