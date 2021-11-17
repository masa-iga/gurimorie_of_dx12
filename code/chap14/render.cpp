#include "render.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cassert>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <synchapi.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "pmd_actor.h"
#include "util.h"

#pragma comment(lib, "DirectXTex.lib")

#define NO_UPDATE_TEXTURE_FROM_CPU (1)

using namespace Microsoft::WRL;

namespace {
	constexpr float kClearColorRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr float kClearColorPeraRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr DirectX::XMFLOAT4 kPlaneVec(0.0f, 1.0f, 0.0f, 0.0f);
	constexpr DirectX::XMFLOAT3 kParallelLightVec(1.0f, -1.0f, 1.0f);

	HRESULT createFence(UINT64 initVal, ComPtr<ID3D12Fence>* fence);
	HRESULT createDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* descHeap, ComPtr<ID3D12DescriptorHeap>* srvDescHeap);
	HRESULT createLightDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* dsvHeap, ComPtr<ID3D12DescriptorHeap>* srvHeap);
	DirectX::XMFLOAT3 getAutoMoveEyePos(bool update, bool reverse);
	DirectX::XMFLOAT3 getAutoMoveLightPos();
	void moveForward(DirectX::XMFLOAT3* focus, DirectX::XMFLOAT3* eye, float amplitude);
	void move(DirectX::XMFLOAT3* dst, DirectX::XMFLOAT3* src, float angle, float amplitude);
	DirectX::XMFLOAT3 computeRotation(DirectX::XMFLOAT3 dst, DirectX::XMFLOAT3 src, DirectX::XMFLOAT3 axis, float angle);
} // namespace anonymous

void Render::onNotify(UiEvent uiEvent, bool flag)
{
	switch (uiEvent) {
	case UiEvent::kUpdateAutoMovePos: m_bAutoMoveEyePos = flag; break;
	case UiEvent::kUpdateAutoLightPos: m_bAutoMoveLightPos = flag; break;
	default: DebugOutputFormatString("unhandled UI event. (%d)\n", uiEvent); ThrowIfFalse(false);
	}
}

HRESULT Render::init(HWND hwnd)
{
	m_parallelLightVec = kParallelLightVec;
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));

	ThrowIfFailed(createDepthBuffer(&m_depthResource, &m_dsvHeap, &m_depthSrvHeap));
	ThrowIfFailed(createLightDepthBuffer(&m_lightDepthResource, &m_lightDepthDsvHeap, &m_lightDepthSrvHeap));
	ThrowIfFailed(createSceneMatrixBuffer());
	ThrowIfFailed(createViews());

	m_floor.init();

	m_pmdActors.resize(1);
	ThrowIfFailed(m_pmdActors[0].loadAsset(PmdActor::Model::kMiku));

	for (auto& actor : m_pmdActors)
	{
		actor.enableAnimation(m_bAnimationEnabled);
	}

	ThrowIfFailed(createPeraView());
	ThrowIfFailed(m_pera.createResources());
	ThrowIfFailed(m_pera.compileShaders());
	ThrowIfFailed(m_pera.createPipelineState());

	ThrowIfFailed(m_shadow.init());
	ThrowIfFailed(m_imguif.init(hwnd));
	m_imguif.addObserver(this);

	ThrowIfFailed(m_timeStamp.init());

	return S_OK;
}

void Render::teardown()
{
	m_imguif.teardown();
	m_imguif.removeObserver(this);
}

HRESULT Render::update()
{
	updateMvpMatrix(m_bAnimationReversed);

	for (auto& actor : m_pmdActors)
		actor.update(m_bAnimationReversed);

	return S_OK;
}

HRESULT Render::render()
{
	ID3D12CommandAllocator* const allocator = Resource::instance()->getCommandAllocator();
	ID3D12GraphicsCommandList* const list = Resource::instance()->getCommandList();
	ID3D12CommandQueue* const queue = Resource::instance()->getCommandQueue();

	ID3D12Resource* backBufferResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = { };
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	{
		const UINT bbIdx = Resource::instance()->getSwapChain()->GetCurrentBackBufferIndex();

		backBufferResource = Resource::instance()->getFrameBuffer(bbIdx);

		rtvH = Resource::instance()->getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * static_cast<SIZE_T>(Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	}

	// reset command allocator & list
	{
		ThrowIfFailed(allocator->Reset());
		ThrowIfFailed(list->Reset(allocator, nullptr));
	}

	// clear
	{
		m_imguif.newFrame();
		clearRenderTarget(list, backBufferResource, rtvH);
		clearDepthRenderTarget(list, dsvH);
		clearDepthRenderTarget(list, m_lightDepthDsvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
		clearPeraRenderTarget(list);
	}

	// render light depth map
	{
		m_floor.renderShadow(list, m_sceneDescHeap.Get(), m_lightDepthDsvHeap.Get());

		for (const auto& actor : m_pmdActors)
		{
			actor.renderShadow(list, m_sceneDescHeap.Get(), m_lightDepthDsvHeap.Get());
		}
	}

	// render to off screen buffer
	preRenderToPeraBuffer(list);
	{
		m_floor.render(list, m_sceneDescHeap.Get(), m_lightDepthSrvHeap.Get());
		m_floor.renderAxis(list, m_sceneDescHeap.Get());

		for (const auto& actor : m_pmdActors)
		{
			actor.render(list, m_sceneDescHeap.Get(), m_lightDepthSrvHeap.Get());
		}
	}
	postRenderToPeraBuffer(list);

	// render to display buffer
	{
		m_timeStamp.set(TimeStamp::Index::k0);
		{
			m_pera.render(&rtvH, m_peraSrvHeap.Get());
		}
		m_timeStamp.set(TimeStamp::Index::k1);
	}

	constexpr bool bDebugRenderShadowMap = true;
	constexpr bool bDebugRenderDepth = true;

	if (bDebugRenderShadowMap)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.f, 0.f, Config::kWindowWidth / 4, Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		m_shadow.render(list, &rtvH, m_lightDepthSrvHeap, viewport, scissorRect);
	}

	if (bDebugRenderDepth)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(Config::kWindowWidth * 3 / 4, 0, Config::kWindowWidth / 4, Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight / 4);
		m_shadow.render(list, &rtvH, m_depthSrvHeap, viewport, scissorRect);
	}

	// render imgui
	{
		m_imguif.build();
		m_imguif.render(Resource::instance()->getCommandList());
	}

	// resolve time stamps
	{
		m_timeStamp.resolve();
	}

	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = backBufferResource;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	}
	list->ResourceBarrier(1, &barrier);


	// execute command lists
	ThrowIfFailed(list->Close());

	ID3D12CommandList* cmdLists[] = { list };
	queue->ExecuteCommandLists(1, cmdLists);

	return S_OK;
}

HRESULT Render::waitForEndOfRendering()
{
	m_fenceVal++;
	ThrowIfFailed(Resource::instance()->getCommandQueue()->Signal(m_pFence.Get(), m_fenceVal));

	while (m_pFence->GetCompletedValue() < m_fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		if (event == nullptr)
		{
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}

		ThrowIfFailed(m_pFence->SetEventOnCompletion(m_fenceVal, event));

		auto ret = WaitForSingleObject(event, INFINITE);
		ThrowIfFalse(ret == WAIT_OBJECT_0);

		BOOL ret2 = CloseHandle(event); // fail����

		if (!ret2)
		{
			DebugOutputFormatString("failed to close handle. (ret %d error %d)\n", ret2, GetLastError());
			ThrowIfFalse(FALSE);
		}
	}

	{
		const float usec = m_timeStamp.getInUsec(TimeStamp::Index::k0, TimeStamp::Index::k1);
		//DebugOutputFormatString("%6.1f usec\n", usec);
	}

	return S_OK;
}

HRESULT Render::swap()
{
	ThrowIfFailed(Resource::instance()->getSwapChain()->Present(1, 0));

	return S_OK;
}

void Render::toggleAnimationEnable()
{
	m_bAnimationEnabled = !m_bAnimationEnabled;

	for (auto& actors : m_pmdActors)
		actors.enableAnimation(m_bAnimationEnabled);
}

void Render::toggleAnimationReverse()
{
	m_bAnimationReversed = !m_bAnimationReversed;
}

void Render::setFpsInImgui(float fps)
{
	m_imguif.setFps(fps);
}

void Render::moveEye(MoveEye moveEye)
{
	switch (moveEye) {
	case MoveEye::kNone:
		break;
	case MoveEye::kForward:
		moveForward(&m_focusPos, &m_eyePos, 0.5f);
		break;
	case MoveEye::kBackward:
		moveForward(&m_focusPos, &m_eyePos, -0.5f);
		break;
	case MoveEye::kRight:
		move(&m_focusPos, &m_eyePos, 90.f, 0.03f);
		break;
	case MoveEye::kLeft:
		move(&m_focusPos, &m_eyePos, -90.f, 0.03f);
		break;
	case MoveEye::kClockwise:
		m_focusPos = computeRotation(m_focusPos, m_eyePos, DirectX::XMFLOAT3(0, 1, 0), 0.03f);
		break;
	case MoveEye::kCounterClockwise:
		m_focusPos = computeRotation(m_focusPos, m_eyePos, DirectX::XMFLOAT3(0, 1, 0), -0.03f);
		break;
	case MoveEye::kUp:
		m_eyePos.y += 0.5f;
		m_focusPos.y += 0.5f;
		break;
	case MoveEye::kDown:
		m_eyePos.y -= 0.5f;
		m_focusPos.y -= 0.5f;
		break;
	default:
		ThrowIfFalse(false);
		break;
	}
}

HRESULT Render::createSceneMatrixBuffer()
{
	using namespace DirectX;

	{
		const size_t w = Util::alignmentedSize(sizeof(SceneMatrix), 256);

		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = w;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_sceneMatrixResource.ReleaseAndGetAddressOf())
		);
		ThrowIfFailed(result);

		result = m_sceneMatrixResource.Get()->SetName(Util::getWideStringFromString("renderSceneMatrixBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		auto result = m_sceneMatrixResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&m_sceneMatrix)
		);
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Render::createViews()
{
	// create a descriptor heap for shader resource
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = { };
	{
		{
			descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descHeapDesc.NumDescriptors = 1;
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			descHeapDesc.NodeMask = 0;
		}

		auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&descHeapDesc,
			IID_PPV_ARGS(m_sceneDescHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);

		ret = m_sceneDescHeap.Get()->SetName(Util::getWideStringFromString("renderSceneDescHeap").c_str());
		ThrowIfFailed(ret);
	}

	// create a shader resource view on the heap
	{
		auto basicHeapHandle = m_sceneDescHeap->GetCPUDescriptorHandleForHeapStart();
		D3D12_CONSTANT_BUFFER_VIEW_DESC bvDesc = { };
		{
			bvDesc.BufferLocation = m_sceneMatrixResource->GetGPUVirtualAddress();
			bvDesc.SizeInBytes = static_cast<UINT>(m_sceneMatrixResource->GetDesc().Width);
		}
		Resource::instance()->getDevice()->CreateConstantBufferView(
			&bvDesc,
			basicHeapHandle
		);
	}

	return S_OK;
}

HRESULT Render::createPeraView()
{
	// create resource for render-to-texture
	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
#if NO_UPDATE_TEXTURE_FROM_CPU
			heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
#else
			heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
#endif // NO_UPDATE_TEXTURE_FROM_CPU
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		D3D12_CLEAR_VALUE clearValue = { };
		{
			clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			memcpy(&clearValue.Color, &kClearColorPeraRenderTarget, sizeof(clearValue.Color));
		}

		D3D12_RESOURCE_DESC resDesc = Resource::instance()->getFrameBuffer(0)->GetDesc();

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(m_peraResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_peraResource.Get()->SetName(Util::getWideStringFromString("peraBuffer").c_str());
		ThrowIfFailed(result);
	}

#if NO_UPDATE_TEXTURE_FROM_CPU
#else
	{
		auto desc = m_peraResource->GetDesc();
		const uint32_t width = (uint32_t)desc.Width;
		const uint32_t height = (uint32_t)desc.Height;

		uint32_t* white = new uint32_t[width * height];
		memset(white, 0x80, (size_t)width * height * sizeof(uint32_t));

		auto result = m_peraResource->WriteToSubresource(
			0,
			nullptr,
			white,
			width * sizeof(uint32_t),
			width * height * sizeof(uint32_t));
		ThrowIfFailed(result);

		delete[] white;
	}
#endif // NO_UPDATE_TEXTURE_FROM_CPU

	// create RTV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = Resource::instance()->getRtvHeaps()->GetDesc();
		{
			heapDesc.NumDescriptors = 1;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_peraRtvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_peraRtvHeap.Get()->SetName(Util::getWideStringFromString("peraRtvHeap").c_str());
		ThrowIfFailed(result);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
		{
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		}

		Resource::instance()->getDevice()->CreateRenderTargetView(
			m_peraResource.Get(),
			&rtvDesc,
			m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_peraSrvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_peraSrvHeap.Get()->SetName(Util::getWideStringFromString("peraSrvHeap").c_str());
		ThrowIfFailed(result);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
		{
			srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Resource::instance()->getDevice()->CreateShaderResourceView(
			m_peraResource.Get(),
			&srvDesc,
			m_peraSrvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
}

HRESULT Render::updateMvpMatrix(bool animationReversed)
{
	using namespace DirectX;
	constexpr XMFLOAT3 up(0, 1, 0);

	XMFLOAT3 eyePos(0, 0, 0);
	XMFLOAT3 focusPos(0, 0, 0);

	XMFLOAT3 lightPos(0, 50, -10);
	XMFLOAT3 lightFocusPos(0, 0, 0);

	if (m_bAutoMoveEyePos)
	{
		eyePos = getAutoMoveEyePos(m_bAnimationEnabled, animationReversed);
	}
	else
	{
		eyePos = m_eyePos;
		focusPos = m_focusPos;
	}

	if (m_bAutoMoveLightPos)
	{
		lightPos = getAutoMoveLightPos();
	}

	{
		const XMMATRIX viewMat = XMMatrixLookAtLH(
			XMLoadFloat3(&eyePos),
			XMLoadFloat3(&focusPos),
			XMLoadFloat3(&up)
		);

		m_sceneMatrix->view = viewMat;
	}

	{
		const XMMATRIX projMat = XMMatrixPerspectiveFovLH(
			XM_PIDIV2,
			static_cast<float>(Config::kWindowWidth) / static_cast<float>(Config::kWindowHeight),
			1.0f,
			150.0f
		);

		m_sceneMatrix->proj = projMat;
	}

	{
		constexpr float viewWidth = 50.0f;
		constexpr float viewHeight = 50.0f;
		const XMVECTOR lightVec = XMLoadFloat3(&lightPos);

		m_sceneMatrix->lightCamera =
			XMMatrixLookAtLH(lightVec, XMLoadFloat3(&lightFocusPos), XMLoadFloat3(&up)) *
			XMMatrixOrthographicLH(viewWidth, viewHeight, 1.0f, 150.0f);
	}

	m_sceneMatrix->shadow = XMMatrixShadow(XMLoadFloat4(&kPlaneVec), -XMLoadFloat3(&m_parallelLightVec));
	m_sceneMatrix->eye = eyePos;

	{
		m_imguif.setEyePos(eyePos);
		m_imguif.setFocusPos(focusPos);
		m_imguif.setLightPos(lightPos);
	}

	return S_OK;
}

HRESULT Render::clearRenderTarget(ID3D12GraphicsCommandList* list, ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtvH)
{
	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	list->ResourceBarrier(1, &barrier);

	list->ClearRenderTargetView(rtvH, kClearColorRenderTarget, 0, nullptr);

	return S_OK;
}

HRESULT Render::clearDepthRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dsvH)
{
	list->ClearDepthStencilView(
		dsvH,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
        nullptr);

	return S_OK;
}

HRESULT Render::clearPeraRenderTarget(ID3D12GraphicsCommandList* list)
{
	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = m_peraResource.Get();
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	list->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	list->ClearRenderTargetView(rtvH, kClearColorPeraRenderTarget, 0, nullptr);

	return S_OK;
}

HRESULT Render::preRenderToPeraBuffer(ID3D12GraphicsCommandList* list)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	list->OMSetRenderTargets(
		1,
		&rtvH,
		false,
		&dsvH);

	return S_OK;
}

HRESULT Render::postRenderToPeraBuffer(ID3D12GraphicsCommandList* list)
{
	ID3D12Resource* resource = m_peraResource.Get();

	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	list->ResourceBarrier(1, &barrier);

	return S_OK;
}

namespace {
	HRESULT createFence(UINT64 initVal, ComPtr<ID3D12Fence>* fence)
	{
		return Resource::instance()->getDevice()->CreateFence(
			initVal,
			D3D12_FENCE_FLAG_NONE,
			IID_PPV_ARGS(fence->ReleaseAndGetAddressOf()));
	}

	HRESULT createDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* descHeap, ComPtr<ID3D12DescriptorHeap>* srvDescHeap)
	{
		{
			D3D12_RESOURCE_DESC resourceDesc = { };
			{
				resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resourceDesc.Alignment = 0;
				resourceDesc.Width = Config::kWindowWidth;
				resourceDesc.Height = Config::kWindowHeight;
				resourceDesc.DepthOrArraySize = 1;
				resourceDesc.MipLevels = 1;
				resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS; // should be DXGI_FORMAT_D32_FLOAT because the buffer will be read as a texture
				resourceDesc.SampleDesc = { 1, 0 };
				resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}

			D3D12_HEAP_PROPERTIES heapProp = { };
			{
				heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
				heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapProp.CreationNodeMask = 1;
				heapProp.VisibleNodeMask = 1;
			}

			D3D12_CLEAR_VALUE clearVal = { };
			{
				clearVal.Format = DXGI_FORMAT_D32_FLOAT;
				clearVal.DepthStencil.Depth = 1.0f;
				clearVal.DepthStencil.Stencil = 0;
			}

			auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clearVal,
				IID_PPV_ARGS(resource->ReleaseAndGetAddressOf()));
			ThrowIfFailed(ret);

			ret = resource->Get()->SetName(Util::getWideStringFromString("depthBuffer").c_str());
			ThrowIfFailed(ret);
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = { };
			{
				descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				descHeapDesc.NumDescriptors = 1;
				descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				descHeapDesc.NodeMask = 0;
			}

			auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(
				&descHeapDesc,
				IID_PPV_ARGS(descHeap->ReleaseAndGetAddressOf()));
			ThrowIfFailed(ret);

			ret = descHeap->Get()->SetName(Util::getWideStringFromString("depthHeap").c_str());
			ThrowIfFailed(ret);
		}

		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { };
			{
				dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
				dsvDesc.Texture2D.MipSlice = 0;
			}

			Resource::instance()->getDevice()->CreateDepthStencilView(
				resource->Get(),
				&dsvDesc,
				descHeap->Get()->GetCPUDescriptorHandleForHeapStart());
		}


		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
			{
				heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				heapDesc.NumDescriptors = 1;
				heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				heapDesc.NodeMask = 0;
			}

			auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(
				&heapDesc,
				IID_PPV_ARGS(srvDescHeap->ReleaseAndGetAddressOf()));
			ThrowIfFailed(ret);
		}

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
			{
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0;
			}

			Resource::instance()->getDevice()->CreateShaderResourceView(
				resource->Get(),
				&srvDesc,
				srvDescHeap->Get()->GetCPUDescriptorHandleForHeapStart());
		}

		return S_OK;
	}

	HRESULT createLightDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* dsvHeap, ComPtr<ID3D12DescriptorHeap>* srvHeap)
	{
		constexpr uint32_t kShadowBufferWidth = 1024;
		constexpr uint32_t kShadowBufferHeight = kShadowBufferWidth;

		{
			D3D12_HEAP_PROPERTIES heapProp = { };
			{
				heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
				heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				heapProp.CreationNodeMask = 0;
				heapProp.VisibleNodeMask = 0;
			}

			D3D12_RESOURCE_DESC resourceDesc = { };
			{
				resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				resourceDesc.Alignment = 0;
				resourceDesc.Width = kShadowBufferWidth;
				resourceDesc.Height = kShadowBufferHeight;
				resourceDesc.DepthOrArraySize = 1;
				resourceDesc.MipLevels = 1;
				resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				resourceDesc.SampleDesc = { 1, 0 };
				resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}

			D3D12_CLEAR_VALUE clearVal = { };
			{
				clearVal.Format = DXGI_FORMAT_D32_FLOAT;
				clearVal.DepthStencil.Depth = 1.0f;
				clearVal.DepthStencil.Stencil = 0;
			}

			auto result = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&clearVal,
				IID_PPV_ARGS(resource->ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = resource->Get()->SetName(Util::getWideStringFromString("lightDepthBuffer").c_str());
			ThrowIfFailed(result);
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
			{
				heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				heapDesc.NumDescriptors = 1;
				heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				heapDesc.NodeMask = 0;
			}

			auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
				&heapDesc,
				IID_PPV_ARGS(dsvHeap->ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = dsvHeap->Get()->SetName(Util::getWideStringFromString("lightDepthBufferDsvHeap").c_str());
			ThrowIfFailed(result);
		}

		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { };
			{
				dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
				dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
				dsvDesc.Texture2D.MipSlice = 0;
			}

			Resource::instance()->getDevice()->CreateDepthStencilView(
				resource->Get(),
				&dsvDesc,
				dsvHeap->Get()->GetCPUDescriptorHandleForHeapStart());
		}

		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
			{
				heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				heapDesc.NumDescriptors = 1;
				heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				heapDesc.NodeMask = 0;
			}

			auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
				&heapDesc,
				IID_PPV_ARGS(srvHeap->ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = srvHeap->Get()->SetName(Util::getWideStringFromString("lightDepthBufferSrvHeap").c_str());
			ThrowIfFailed(result);
		}

		{
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
			{
				srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Texture2D.MostDetailedMip = 0;
				srvDesc.Texture2D.MipLevels = 1;
				srvDesc.Texture2D.PlaneSlice = 0;
				srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			}

			Resource::instance()->getDevice()->CreateShaderResourceView(
				resource->Get(),
				&srvDesc,
				srvHeap->Get()->GetCPUDescriptorHandleForHeapStart());
		}

		return S_OK;
	}

	DirectX::XMFLOAT3 getAutoMoveEyePos(bool update, bool reverse)
	{
		static float angle = 0.0f;
		constexpr float kRadius = 30.0f;

		const float x = kRadius * std::sin(angle);
		const float y = 20.0f;
		const float z = -1 * kRadius * std::cos(angle);

		if (!update)
		{
			;
		}
		else if (!reverse)
		{
			angle += 0.01f;
		}
		else
		{
			angle -= 0.01f;
		}

		return DirectX::XMFLOAT3(x, y, z);
	}

	DirectX::XMFLOAT3 getAutoMoveLightPos()
	{
		static float angle = 0.0f;
		constexpr float kRadius = 15.0f;

		const float x = kRadius * std::cos(angle);
		const float y = 15.0f;
		const float z = kRadius * std::sin(angle);
		angle -= 0.005f;

		return DirectX::XMFLOAT3(x, y, z);
	}

	void moveForward(DirectX::XMFLOAT3* focus, DirectX::XMFLOAT3* eye, float amplitude)
	{
		const DirectX::XMFLOAT3 vec(focus->x - eye->x, focus->y - eye->y, focus->z - eye->z);
		const float abs = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
		const DirectX::XMFLOAT3 normalizedVec(vec.x / abs, vec.y / abs, vec.z / abs);

		*focus = { focus->x + amplitude * normalizedVec.x, focus->y + amplitude * normalizedVec.y, focus->z + amplitude * normalizedVec.z };
		*eye = { eye->x + amplitude * normalizedVec.x, eye->y + amplitude * normalizedVec.y, eye->z + amplitude * normalizedVec.z };
	}

	void move(DirectX::XMFLOAT3* dst, DirectX::XMFLOAT3* src, float angle, float amplitude)
	{
		const DirectX::XMFLOAT3 vec(dst->x - src->x, dst->y - src->y, dst->z - src->z);

		// We assume that rotations will be done clockwise along Y axis
		constexpr DirectX::XMFLOAT3 axis(0, 1, 0);

		const float radian = DirectX::XMConvertToRadians(angle);
		const DirectX::XMVECTOR rotQuaternion = DirectX::XMQuaternionRotationAxis(DirectX::XMLoadFloat3(&axis), radian);

		const DirectX::XMVECTOR rotVec = DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&vec), rotQuaternion);

		DirectX::XMFLOAT3 rotPos = { };
		DirectX::XMStoreFloat3(&rotPos, rotVec);

		*dst = { dst->x + amplitude * rotPos.x, dst->y + amplitude * rotPos.y, dst->z + amplitude * rotPos.z };
		*src = { src->x + amplitude * rotPos.x, src->y + amplitude * rotPos.y, src->z + amplitude * rotPos.z };
	};

	DirectX::XMFLOAT3 computeRotation(DirectX::XMFLOAT3 dst, DirectX::XMFLOAT3 src, DirectX::XMFLOAT3 axis, float angle)
	{
		const DirectX::XMFLOAT3 vec(dst.x - src.x, dst.y - src.y, dst.z - src.z);
		const DirectX::XMVECTOR rotQuaternion = DirectX::XMQuaternionRotationAxis(DirectX::XMLoadFloat3(&axis), angle);
		const DirectX::XMVECTOR rotVec = DirectX::XMVector3Rotate(DirectX::XMLoadFloat3(&vec), rotQuaternion);

		DirectX::XMFLOAT3 temp = { };
		DirectX::XMStoreFloat3(&temp, rotVec);

		return DirectX::XMFLOAT3(temp.x + src.x, temp.y + src.y, temp.z + src.z);
	};

} // namespace anonymous