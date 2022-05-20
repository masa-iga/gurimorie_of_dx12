#include "render.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cassert>
#include <d3dx12.h>
#include <functional>
#include <DirectXMath.h>
#include <synchapi.h>
#pragma warning(pop)
#include "config.h"
#include "constant.h"
#include "debug.h"
#include "init.h"
#include "pixif.h"
#include "pmd_actor.h"
#include "util.h"

#pragma comment(lib, "DirectXTex.lib")

#define NO_UPDATE_TEXTURE_FROM_CPU (1)

using namespace Microsoft::WRL;

Toolkit Render::s_toolkit = Toolkit();

namespace {
	constexpr float kClearColorRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr float kClearColorPeraRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr DirectX::XMFLOAT4 kPlaneVec(0.0f, 1.0f, 0.0f, 0.0f);
	constexpr DirectX::XMFLOAT3 kParallelLightVec(1.0f, -1.0f, 1.0f);

	HRESULT createFence(UINT64 initVal, ComPtr<ID3D12Fence>* fence);
	HRESULT createDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* descHeap, ComPtr<ID3D12DescriptorHeap>* srvDescHeap);
	HRESULT createLightDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* dsvHeap, ComPtr<ID3D12DescriptorHeap>* srvHeap);
	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE handle, const float col[4]);
	DirectX::XMFLOAT3 getAutoMoveEyePos(bool update, bool reverse);
	DirectX::XMFLOAT3 getAutoMoveLightPos();
	void moveForward(DirectX::XMFLOAT3* focus, DirectX::XMFLOAT3* eye, float amplitude);
	void rotateAroundXaxis(DirectX::XMFLOAT3* focus, DirectX::XMFLOAT3* eye, float amplitude);
	DirectX::XMFLOAT3 computeRotation(DirectX::XMFLOAT3 dst, DirectX::XMFLOAT3 src, DirectX::XMFLOAT3 axis, float angle);
} // namespace anonymous

HRESULT OffScreenResource::createResource(DXGI_FORMAT format)
{
	// create resource for render-to-texture
	{
#if NO_UPDATE_TEXTURE_FROM_CPU
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, 0, 0);
#else
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 0, 0);
#endif // NO_UPDATE_TEXTURE_FROM_CPU

		D3D12_RESOURCE_DESC resDesc = Resource::instance()->getFrameBuffer(0)->GetDesc();

		for (size_t i = 0; auto& resource : m_resources)
		{
			const D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(format, kClearColor.at(i));

			auto result = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&clearValue,
				IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = resource.Get()->SetName(Util::getWideStringFromString("baseBuffer" + std::to_string(i)).c_str());

			++i;
		}
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
			heapDesc.NumDescriptors = kNumResource;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_rtvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_rtvHeap.Get()->SetName(Util::getWideStringFromString("baseRtvHeap").c_str());
		ThrowIfFailed(result);
	}
	// create RTV views
	{
		const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
			.Format = format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
		};

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		for (auto& resource : m_resources)
		{
			Resource::instance()->getDevice()->CreateRenderTargetView(
				resource.Get(),
				&rtvDesc,
				handle);
			handle.ptr += Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
	}

	// create SRV heap
	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = kNumResource,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_srvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_srvHeap.Get()->SetName(Util::getWideStringFromString("baseSrvHeap").c_str());
		ThrowIfFailed(result);
	}
	// create SR views
	{
		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = format,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1,
				.PlaneSlice = 0,
				.ResourceMinLODClamp = 0.0f,
			},
		};

		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		for (auto& resource : m_resources)
		{
			Resource::instance()->getDevice()->CreateShaderResourceView(
				resource.Get(),
				&srvDesc,
				handle);
			handle.ptr += Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}
	}

	return S_OK;
}

HRESULT OffScreenResource::clearRenderTargets(ID3D12GraphicsCommandList* list) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	for (size_t i = 0; i < m_resources.size(); ++i)
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_resources.at(i).Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			0);

		list->ResourceBarrier(1, &barrier);
		list->ClearRenderTargetView(handle, kClearColor.at(i), 0, nullptr);

		handle.ptr += Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return S_OK;
}

HRESULT OffScreenResource::buildBarrier(ID3D12GraphicsCommandList* list, Type type, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) const
{
	const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_resources.at(static_cast<size_t>(type)).Get(),
		stateBefore,
		stateAfter);
	list->ResourceBarrier(1, &barrier);

	return S_OK;
}

HRESULT OffScreenResource::buildBarrier(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) const
{
	for (size_t i = 0; i < kNumResource; ++i)
	{
		auto result = buildBarrier(list, static_cast<Type>(i), stateBefore, stateAfter);
		ThrowIfFailed(result);
	}

	return S_OK;
}

Microsoft::WRL::ComPtr<ID3D12Resource> OffScreenResource::getResource(Type type) const
{
	return m_resources.at(static_cast<size_t>(type));
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> OffScreenResource::getRtvHeap() const
{
	return m_rtvHeap;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> OffScreenResource::getSrvHeap() const
{
	return m_srvHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE OffScreenResource::getRtvCpuDescHandle(Type type) const
{
	const UINT incSize = Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap.Get()->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(type), incSize);
	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE OffScreenResource::getSrvGpuDescHandle(Type type) const
{
	const UINT incSize = Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_srvHeap.Get()->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(type), incSize);
	return handle;
}

void Render::onNotify(UiEvent uiEvent, const void* uiEventData)
{
	switch (uiEvent) {
	case UiEvent::kUpdateAutoMovePos:
		m_bAutoMoveEyePos = reinterpret_cast<const UiEventDataUpdateAutoMovePos*>(uiEventData)->flag;
		break;
	case UiEvent::kUpdateAutoLightPos:
		m_bAutoMoveLightPos = reinterpret_cast<const UiEventDataUpdateAutoLightPos*>(uiEventData)->flag;
		break;
	case UiEvent::kUpdateHighLuminanceThreshold:
		updateHighLuminanceThreshold(reinterpret_cast<const UiEventDataUpdateHighLuminanceThreshold*>(uiEventData)->val);
		break;
	default:
		Debug::debugOutputFormatString("unhandled UI event. (%d)\n", uiEvent);
		ThrowIfFalse(false);
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

	ThrowIfFailed(CommonResource::init());
	ThrowIfFailed(s_toolkit.init());

	m_pmdActors.resize(1);
	ThrowIfFailed(m_pmdActors[0].loadAsset(PmdActor::Model::kMiku));

	for (auto& actor : m_pmdActors)
	{
		actor.enableAnimation(m_bAnimationEnabled);
	}

	ThrowIfFailed(m_offScreenResource.createResource(Constant::kDefaultRtFormat));
	ThrowIfFailed(m_pera.createResources());
	ThrowIfFailed(m_pera.compileShaders());
	ThrowIfFailed(m_pera.createPipelineState());

	ThrowIfFailed(m_floor.init());
	ThrowIfFailed(m_bloom.init(Config::kWindowWidth, Config::kWindowHeight));
	ThrowIfFailed(m_dof.init(Config::kWindowWidth, Config::kWindowHeight));
	ThrowIfFailed(m_ssao.init(Config::kWindowWidth, Config::kWindowHeight));
	ThrowIfFailed(m_shadow.init());
	ThrowIfFailed(m_graph.init());
	ThrowIfFailed(m_imguif.init(hwnd));
	m_imguif.addObserver(this);
	ThrowIfFailed(initEffekseer());
	ThrowIfFailed(m_effekseerProxy.load());
	ThrowIfFailed(m_effekseerProxy.play());
	ThrowIfFailed(m_dxtkIf.init(Resource::instance()->getDevice(), Constant::kDefaultRtFormat, Constant::kDefaultDrtFormat));
	{
		auto f = std::bind(&Render::waitForEndOfRenderingInternal, this, std::placeholders::_1);
		ThrowIfFailed(m_dxtkIf.upload(Resource::instance()->getCommandQueue(), f));
	}

	ThrowIfFailed(m_timeStamp.init());

	return S_OK;
}

void Render::teardown()
{
	CommonResource::tearDown();
	s_toolkit.teardown();
	m_imguif.teardown();
	m_imguif.removeObserver(this);
}

HRESULT Render::update()
{
	updateMvpMatrix(m_bAnimationReversed);

	for (auto& actor : m_pmdActors)
	{
		actor.update(m_bAnimationReversed);
	}

	m_graph.set(m_timeStamp.getInUsec(TimeStamp::Index::k0, TimeStamp::Index::k3) / 1000.0f);
	m_graph.update();
	m_effekseerProxy.update();

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

	// wait unti the back buffer is available
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBufferResource,
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			0);
		list->ResourceBarrier(1, &barrier);
	}

	{
		m_imguif.setRenderTime(m_timeStamp.getInUsec(TimeStamp::Index::k0, TimeStamp::Index::k3) / 1000.0f);
	}

	// get time stamp for starting
	{
		m_timeStamp.set(list, TimeStamp::Index::k0);
	}

	// clear buffers
	{
		const PixScopedEvent pixScopedEvent(list, "clearBuffers");

		m_imguif.newFrame();
		clearRenderTarget(list, rtvH, kClearColorRenderTarget);
		clearDepthRenderTargets(list);
		m_offScreenResource.clearRenderTargets(list);
		m_ssao.clearRenderTarget(list);
		m_bloom.clearWorkRenderTarget(list);
		m_dof.clearWorkRenderTarget(list);
	}

	renderShadowPass(list);

	{
		m_timeStamp.set(list, TimeStamp::Index::k1);
	}

	renderBasePass(list);

	{
		m_timeStamp.set(list, TimeStamp::Index::k2);
	}

	renderPostPass(list, rtvH);

	{
		const PixScopedEvent pixScopedEvent(list, "Effekseer");
		m_effekseerProxy.draw(list);
	}

	// UI: axis
	{
		const PixScopedEvent pixScopedEvent(list, "Axis");

		{
			const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				m_depthResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_READ,
				0);
			list->ResourceBarrier(1, &barrier);
		}

		m_floor.renderAxis(list, m_sceneDescHeap.Get(), rtvH, dsvH);
	}

	renderDebugPass(list, &rtvH);

	// UI: render imgui
	{
		const PixScopedEvent pixScopedEvent(list, "IMGUI");
		m_imguif.build();
		m_imguif.render(list);
	}

	// Text
	{
		const PixScopedEvent pixScopedEvent(list, "Text");

		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		list->RSSetScissorRects(1, &scissorRect);

		const D3D12_VIEWPORT viewPort = CD3DX12_VIEWPORT(
			0.0f,
			0.0f,
			Config::kWindowWidth,
			Config::kWindowHeight);

		m_dxtkIf.setViewport(viewPort);
		ThrowIfFailed(m_dxtkIf.draw(list, Config::kWindowWidth / 2.0f, Config::kWindowHeight / 2.0f));
	}

	// time stamp for ending
	{
		m_timeStamp.set(list, TimeStamp::Index::k3);
	}

	// resolve time stamps
	{
		m_timeStamp.resolve(list);
	}

	resolveResourceBarrier(list, backBufferResource);

	// execute command lists
	ThrowIfFailed(list->Close());

	ID3D12CommandList* cmdLists[] = { list };
	queue->ExecuteCommandLists(1, cmdLists);

	return S_OK;
}

HRESULT Render::waitForEndOfRendering()
{
	return waitForEndOfRenderingInternal(Resource::instance()->getCommandQueue());
}

HRESULT Render::swap()
{
	ThrowIfFailed(Resource::instance()->getSwapChain()->Present(1, 0));
	ThrowIfFailed(m_dxtkIf.commit(Resource::instance()->getCommandQueue()));

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

void Render::moveEye(MoveEye moveEye, float val)
{
	switch (moveEye) {
	case MoveEye::kNone:
		break;
	case MoveEye::kPosX:
		rotateAroundXaxis(&m_focusPos, &m_eyePos, val);
		break;
	case MoveEye::kPosY:
		m_eyePos.y += val;
		m_focusPos.y += val;
		break;
	case MoveEye::kPosZ:
		moveForward(&m_focusPos, &m_eyePos, val);
		break;
	case MoveEye::kFocusX:
		m_focusPos = computeRotation(m_focusPos, m_eyePos, DirectX::XMFLOAT3(0, 1, 0), val);
		break;
	case MoveEye::kFocusY:
		m_focusPos = computeRotation(m_focusPos, m_eyePos, DirectX::XMFLOAT3(-1, 0, 0), val);
		break;
	default:
		ThrowIfFalse(false);
		break;
	}
}

HRESULT Render::initEffekseer()
{
	DXGI_FORMAT rtFormats[] = { Resource::instance()->getFrameBuffer(0)->GetDesc().Format };
	constexpr DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
	constexpr bool bReverseDepth = false;
	constexpr int32_t kSquareMaxCount = 10'000;

	ThrowIfFailed(m_effekseerProxy.init(
		Resource::instance()->getDevice(),
		Resource::instance()->getCommandQueue(),
		2,
		rtFormats,
		_countof(rtFormats),
		depthFormat,
		bReverseDepth,
		kSquareMaxCount));

	return S_OK;
}

HRESULT Render::createSceneMatrixBuffer()
{
	using namespace DirectX;

	{
		const size_t w = Util::alignmentedSize(sizeof(SceneParam), 256);

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
			IID_PPV_ARGS(m_sceneParamResource.ReleaseAndGetAddressOf())
		);
		ThrowIfFailed(result);

		result = m_sceneParamResource.Get()->SetName(Util::getWideStringFromString("renderSceneParamBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		auto result = m_sceneParamResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&m_sceneParam)
		);
		ThrowIfFailed(result);

		// init
		m_sceneParam->highLuminanceThreshold = Config::kDefaultHighLuminanceThreshold;
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
			bvDesc.BufferLocation = m_sceneParamResource->GetGPUVirtualAddress();
			bvDesc.SizeInBytes = static_cast<UINT>(m_sceneParamResource->GetDesc().Width);
		}
		Resource::instance()->getDevice()->CreateConstantBufferView(
			&bvDesc,
			basicHeapHandle
		);
	}

	return S_OK;
}

HRESULT Render::updateMvpMatrix(bool animationReversed)
{
	using namespace DirectX;
	constexpr XMFLOAT3 up(0, 1, 0);

	XMFLOAT3 eyePos(0, 0, 0);
	XMFLOAT3 focusPos(0, 0, 0);

	//XMFLOAT3 lightPos(0, 50, -10);
	XMFLOAT3 lightPos(0, 10, -20);
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

		m_sceneParam->view = viewMat;
	}

	{
		const XMMATRIX projMat = XMMatrixPerspectiveFovLH(
			XM_PIDIV2,
			static_cast<float>(Config::kWindowWidth) / static_cast<float>(Config::kWindowHeight),
			1.0f,
			150.0f
		);

		m_sceneParam->proj = projMat;
		m_sceneParam->invProj = XMMatrixInverse(nullptr, projMat);
	}

	{
		constexpr float viewWidth = 50.0f;
		constexpr float viewHeight = 50.0f;
		const XMVECTOR lightVec = XMLoadFloat3(&lightPos);

		m_sceneParam->lightCamera =
			XMMatrixLookAtLH(lightVec, XMLoadFloat3(&lightFocusPos), XMLoadFloat3(&up)) *
			XMMatrixOrthographicLH(viewWidth, viewHeight, 1.0f, 150.0f);
	}

	m_sceneParam->shadow = XMMatrixShadow(XMLoadFloat4(&kPlaneVec), -XMLoadFloat3(&m_parallelLightVec));
	m_sceneParam->eye = eyePos;

	{
		m_imguif.setEyePos(eyePos);
		m_imguif.setFocusPos(focusPos);
		m_imguif.setLightPos(lightPos);
	}

	{
		m_effekseerProxy.setCameraMatrix(m_sceneParam->view);
		m_effekseerProxy.setProjectionMatrix(m_sceneParam->proj);
	}

	return S_OK;
}

void Render::updateHighLuminanceThreshold(float val)
{
	m_sceneParam->highLuminanceThreshold = val;
}

HRESULT Render::clearDepthRenderTargets(ID3D12GraphicsCommandList* list)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE depthHandles[] = {
		m_dsvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_lightDepthDsvHeap.Get()->GetCPUDescriptorHandleForHeapStart(),
	};

	{
		const CD3DX12_RESOURCE_BARRIER barriers[] = {
			{
				CD3DX12_RESOURCE_BARRIER::Transition(
				m_depthResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				0)
			},
			{
				CD3DX12_RESOURCE_BARRIER::Transition(
				m_lightDepthResource.Get(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				0)
			},
		};

		list->ResourceBarrier(_countof(barriers), barriers);
	}

	for (const auto &handle : depthHandles)
	{
		list->ClearDepthStencilView(
			handle,
			D3D12_CLEAR_FLAG_DEPTH,
			1.0f,
			0,
			0,
			nullptr);
	}

	return S_OK;
}

void Render::renderShadowPass(ID3D12GraphicsCommandList* list)
{
	// shadow map: render light depth map
	const PixScopedEvent pixScopedEvent(list, "ShadowMap");

	m_floor.renderShadow(list, m_sceneDescHeap.Get(), m_lightDepthDsvHeap.Get());

	for (const auto& actor : m_pmdActors)
	{
		actor.renderShadow(list, m_sceneDescHeap.Get(), m_lightDepthDsvHeap.Get());
	}
}

void Render::renderBasePass(ID3D12GraphicsCommandList* list)
{
	const PixScopedEvent pixScopedEvent(list, "BasePass");

	// base pass: RT0=albedo, RT1=normal, RT2=luminance, depth
	{
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_lightDepthResource.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		list->ResourceBarrier(1, &barrier);
	}

	{
		const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 3> rtvHandles = {
			m_offScreenResource.getRtvCpuDescHandle(OffScreenResource::Type::kColor),
			m_offScreenResource.getRtvCpuDescHandle(OffScreenResource::Type::kNormal),
			m_offScreenResource.getRtvCpuDescHandle(OffScreenResource::Type::kLuminance),
		};
		const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(
			static_cast<UINT>(rtvHandles.size()),
			rtvHandles.data(),
			false,
			&dsvHandle);
	}

	{
		m_floor.render(list, m_sceneDescHeap.Get(), m_lightDepthSrvHeap.Get());

		for (const auto& actor : m_pmdActors)
		{
			actor.render(list, m_sceneDescHeap.Get(), m_lightDepthSrvHeap.Get());
		}
	}
}

void Render::renderPostPass(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE fbRtvHandle)
{
	const PixScopedEvent pixScopedEvent(list, "PostProcess");

	// resource barrier
	{
		m_offScreenResource.buildBarrier(list, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_depthResource.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			0);
		list->ResourceBarrier(1, &barrier);
	}

	// post process: SSAO
	{
		const PixScopedEvent pixScopedEvent(list, "PostProcess : SSAO");

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostSsao,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		m_ssao.setResource(Ssao::TargetResource::kDstRt, m_offScreenResource.getResource(OffScreenResource::Type::kPostSsao));
		m_ssao.setResource(Ssao::TargetResource::kSrcDepth, m_depthResource);
		m_ssao.setResource(Ssao::TargetResource::kSrcNormal, m_offScreenResource.getResource(OffScreenResource::Type::kNormal));
		m_ssao.setResource(Ssao::TargetResource::kSrcColor, m_offScreenResource.getResource(OffScreenResource::Type::kColor));
		m_ssao.setResource(Ssao::TargetResource::kSrcSceneParam, m_sceneParamResource);

		m_ssao.render(list);

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostSsao,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// post process: bloom
	{
		const PixScopedEvent pixScopedEvent(list, "PostProcess : bloom");

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostBloom,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		m_bloom.renderShrinkTextureForBlur(
			list,
			m_offScreenResource.getSrvHeap().Get(),
			m_offScreenResource.getSrvGpuDescHandle(OffScreenResource::Type::kLuminance));
		m_bloom.render(
			list,
			m_offScreenResource.getRtvCpuDescHandle(OffScreenResource::Type::kPostBloom),
			m_offScreenResource.getSrvHeap().Get(),
			m_offScreenResource.getSrvGpuDescHandle(OffScreenResource::Type::kPostSsao),
			m_offScreenResource.getSrvGpuDescHandle(OffScreenResource::Type::kLuminance));

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostBloom,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// post process: depth of field
	{
		const PixScopedEvent pixScopedEvent(list, "PostProcess : DoF");

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostDof,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		m_dof.render(
			list,
			m_offScreenResource.getRtvCpuDescHandle(OffScreenResource::Type::kPostDof),
			m_offScreenResource.getSrvHeap().Get(),
			m_offScreenResource.getSrvGpuDescHandle(OffScreenResource::Type::kPostBloom),
			m_depthSrvHeap.Get(),
			m_depthSrvHeap.Get()->GetGPUDescriptorHandleForHeapStart());

		m_offScreenResource.buildBarrier(
			list,
			OffScreenResource::Type::kPostDof,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	// post process: pera (render to display buffer)
	{
		const PixScopedEvent pixScopedEvent(list, "PostProcess : pera");

		m_pera.render(
			&fbRtvHandle,
			m_offScreenResource.getSrvHeap().Get(),
			m_offScreenResource.getSrvGpuDescHandle(OffScreenResource::Type::kPostDof));
	}
}

void Render::renderDebugPass(ID3D12GraphicsCommandList* list, const D3D12_CPU_DESCRIPTOR_HANDLE* pRtCpuDescHandle)
{
	constexpr bool bDebugRenderShadowMap = true;
	constexpr bool bDebugBloomLuminance = true;
	constexpr bool bDebugRenderDepth = true;
	constexpr bool bDebugNormal = true;
	constexpr bool bDebugGraph = true;

	const PixScopedEvent pixScopedEvent(list, __func__);

	// Debug
	if (bDebugRenderShadowMap)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(
			0.0f,
			0.0f,
			Config::kWindowWidth / 4,
			Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		m_shadow.pushRenderCommand(Shadow::Type::kQuadR, m_lightDepthResource.Get(), viewport, scissorRect);
	}

	if (bDebugBloomLuminance)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(
			0.0f,
			Config::kWindowHeight / 4,
			Config::kWindowWidth / 4,
			Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);

		m_shadow.pushRenderCommand(Shadow::Type::kQuadRgba, m_offScreenResource.getResource(OffScreenResource::Type::kLuminance).Get(), viewport, scissorRect);
	}

	if (bDebugRenderDepth)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(
			Config::kWindowWidth * 3 / 4,
			0.0f,
			Config::kWindowWidth / 4,
			Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		m_shadow.pushRenderCommand(Shadow::Type::kQuadR, m_depthResource.Get(), viewport, scissorRect);
	}

	if (bDebugNormal)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(
			Config::kWindowWidth * 3 / 4,
			Config::kWindowHeight / 4,
			Config::kWindowWidth / 4,
			Config::kWindowHeight / 4);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		const CD3DX12_GPU_DESCRIPTOR_HANDLE texGpuDesc(
			m_offScreenResource.getSrvHeap().Get()->GetGPUDescriptorHandleForHeapStart(),
			1,
			Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		m_shadow.pushRenderCommand(Shadow::Type::kQuadRgba, m_offScreenResource.getResource(OffScreenResource::Type::kNormal).Get(), viewport, scissorRect);
	}

	if (bDebugGraph)
	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(
			0.f,
			Config::kWindowHeight - Config::kWindowHeight / 8,
			Config::kWindowWidth / 4,
			Config::kWindowHeight / 8);
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);

		m_graph.render(list, viewport, scissorRect);
	}

	{
		m_shadow.render(list, *pRtCpuDescHandle);
	}
}

void Render::resolveResourceBarrier(ID3D12GraphicsCommandList* list, ID3D12Resource* backBufferResource)
{
	// make ensure that the back buffer can be presented
	{
		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			backBufferResource,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT,
			0);
		list->ResourceBarrier(1, &barrier);
	}

	{
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_depthResource.Get(),
			D3D12_RESOURCE_STATE_DEPTH_READ,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			0);
		list->ResourceBarrier(1, &barrier);
	}
}

HRESULT Render::waitForEndOfRenderingInternal(ID3D12CommandQueue* queue)
{
	m_fenceVal++;
	ThrowIfFailed(queue->Signal(m_pFence.Get(), m_fenceVal));

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
			Debug::debugOutputFormatString("failed to close handle. (ret %d error %d)\n", ret2, GetLastError());
			ThrowIfFalse(FALSE);
		}
	}

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
				clearVal.Format = Constant::kDefaultDrtFormat;
				clearVal.DepthStencil.Depth = 1.0f;
				clearVal.DepthStencil.Stencil = 0;
			}

			auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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
				dsvDesc.Format = Constant::kDefaultDrtFormat;
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
				clearVal.Format = Constant::kDefaultDrtFormat;
				clearVal.DepthStencil.Depth = 1.0f;
				clearVal.DepthStencil.Stencil = 0;
			}

			auto result = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
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
				dsvDesc.Format = Constant::kDefaultDrtFormat;
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

	HRESULT clearRenderTarget(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE handle, const float col[4])
	{
		list->ClearRenderTargetView(handle, col, 0, nullptr);
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

	void rotateAroundXaxis(DirectX::XMFLOAT3* focus, DirectX::XMFLOAT3* eye, float amplitude)
	{
		const DirectX::XMFLOAT3 axis(0.0f, 1.0f, 0.0f); // Y axis
		const DirectX::XMFLOAT3 eye_vector(focus->x - eye->x, 0.0f, focus->z - eye->z);
		const DirectX::XMVECTOR eye_vector0 = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&eye_vector));

		const DirectX::XMVECTOR mappedXvector = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&axis), eye_vector0);
		DirectX::XMFLOAT3 p = { };
		DirectX::XMStoreFloat3(&p, mappedXvector);
		{
			const float abs = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
			p = { p.x / abs, p.y / abs, p.z / abs }; // normalized
		}

		*focus = { focus->x + amplitude * p.x, focus->y, focus->z + amplitude * p.z };
		*eye = { eye->x + amplitude * p.x, eye->y, eye->z + amplitude * p.z };
	}

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