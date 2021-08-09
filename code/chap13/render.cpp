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

#define DISABLE_MATRIX (0)
#define NO_UPDATE_TEXTURE_FROM_CPU (1)

using namespace Microsoft::WRL;

static HRESULT createFence(UINT64 initVal, ComPtr<ID3D12Fence>* fence);
static HRESULT createDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* descHeap);

constexpr float kClearColorRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
constexpr float kClearColorPeraRenderTarget[] = { 1.0f, 1.0f, 1.0f, 1.0f };
constexpr DirectX::XMFLOAT4 kPlaneVec(0.0f, 1.0f, 0.0f, 0.0f);
constexpr DirectX::XMFLOAT3 kParallelLightVec(1.0f, -1.0f, 1.0f);

HRESULT Render::init()
{
	m_parallelLightVec = kParallelLightVec;

	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));
	m_pmdActors.resize(1);
	ThrowIfFailed(m_pmdActors[0].loadAsset(PmdActor::Model::kMiku));
	ThrowIfFailed(createDepthBuffer(&m_depthResource, &m_dsvHeap));
	ThrowIfFailed(createSceneMatrixBuffer());
	ThrowIfFailed(createViews());

	for (auto& actor : m_pmdActors)
	{
		actor.enableAnimation(m_bAnimationEnabled);
	}

	ThrowIfFailed(createPeraView());
	ThrowIfFailed(m_pera.createResources());
	ThrowIfFailed(m_pera.compileShaders());
	ThrowIfFailed(m_pera.createPipelineState());

	ThrowIfFailed(m_timeStamp.init());

	return S_OK;
}

HRESULT Render::update()
{
	updateMvpMatrix();

	for (auto& actor : m_pmdActors)
		actor.update(m_bAnimationReversed);

	return S_OK;
}

HRESULT Render::render()
{
	// reset command allocator & list
	ThrowIfFailed(Resource::instance()->getCommandAllocator()->Reset());
	ThrowIfFailed(Resource::instance()->getCommandList()->Reset(Resource::instance()->getCommandAllocator(), nullptr));

	ID3D12Resource* backBufferResource = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = { };
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
	{
		const UINT bbIdx = Resource::instance()->getSwapChain()->GetCurrentBackBufferIndex();

		backBufferResource = Resource::instance()->getBackBuffer(bbIdx);

		rtvH = Resource::instance()->getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += bbIdx * static_cast<SIZE_T>(Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
	}

	clearRenderTarget(backBufferResource, rtvH);
	clearDepthRenderTarget(dsvH);
	clearPeraRenderTarget();

	// render to off screen buffer
	preRenderToPeraBuffer();
	{
		for (const auto& actor : m_pmdActors)
		{
			actor.render(m_sceneDescHeap.Get());
		}
	}
	postRenderToPeraBuffer();


	// render to display buffer
	m_timeStamp.set(TimeStamp::Index::k0);
	{
		m_pera.render(&rtvH, m_peraSrvHeap.Get());
	}
	m_timeStamp.set(TimeStamp::Index::k1);


	m_timeStamp.resolve();

	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = backBufferResource;
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	}
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);


	// execute command lists
	ThrowIfFailed(Resource::instance()->getCommandList()->Close());

	ID3D12CommandList* cmdLists[] = { Resource::instance()->getCommandList() };
	Resource::instance()->getCommandQueue()->ExecuteCommandLists(1, cmdLists);

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

		BOOL ret2 = CloseHandle(event); // fail‚·‚é

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

		D3D12_RESOURCE_DESC resDesc = Resource::instance()->getBackBuffer(0)->GetDesc();

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

HRESULT Render::updateMvpMatrix()
{
	using namespace DirectX;

	constexpr XMFLOAT3 eye(0, 15, -15);
	constexpr XMFLOAT3 target(0, 15, 0);
	constexpr XMFLOAT3 up(0, 1, 0);

	const auto viewMat = XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up)
	);

	auto projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
		1.0f,
		100.0f
	);

#if DISABLE_MATRIX
	m_sceneMatrix->view = XMMatrixIdentity();
	m_sceneMatrix->proj = XMMatrixIdentity();
	m_sceneMatrix->eye = XMFLOAT3(0.0f, 0.0f, 0.0f);
#else
	m_sceneMatrix->view = viewMat;
	m_sceneMatrix->proj = projMat;
	m_sceneMatrix->eye = eye;
#endif // DISABLE_MATRIX
	m_sceneMatrix->shadow = XMMatrixShadow(XMLoadFloat4(&kPlaneVec), -XMLoadFloat3(&m_parallelLightVec));

	return S_OK;
}

HRESULT Render::clearRenderTarget(ID3D12Resource* resource, D3D12_CPU_DESCRIPTOR_HANDLE rtvH)
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
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);

	Resource::instance()->getCommandList()->ClearRenderTargetView(rtvH, kClearColorRenderTarget, 0, nullptr);

	return S_OK;
}

HRESULT Render::clearDepthRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE dsvH)
{
	Resource::instance()->getCommandList()->ClearDepthStencilView(
		dsvH,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
        nullptr);

	return S_OK;
}

HRESULT Render::clearPeraRenderTarget()
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
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);

	const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	Resource::instance()->getCommandList()->ClearRenderTargetView(rtvH, kClearColorPeraRenderTarget, 0, nullptr);

	return S_OK;
}

HRESULT Render::preRenderToPeraBuffer()
{
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	Resource::instance()->getCommandList()->OMSetRenderTargets(
		1,
		&rtvH,
		false,
		&dsvH);

	return S_OK;
}

HRESULT Render::postRenderToPeraBuffer()
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
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);

	return S_OK;
}

static HRESULT createFence(UINT64 initVal, ComPtr<ID3D12Fence>* fence)
{
	return Resource::instance()->getDevice()->CreateFence(
		initVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(fence->ReleaseAndGetAddressOf()));
}

HRESULT createDepthBuffer(ComPtr<ID3D12Resource>* resource, ComPtr<ID3D12DescriptorHeap>* descHeap)
{
	{
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = kWindowWidth;
			resourceDesc.Height = kWindowHeight;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
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

	return S_OK;
}

