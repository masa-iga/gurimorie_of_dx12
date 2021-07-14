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

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));
	m_pmdActors.resize(1);
	ThrowIfFailed(m_pmdActors[0].loadAsset(PmdActor::Model::kMiku));
	ThrowIfFailed(createDepthBuffer(&m_depthResource, &m_dsvHeap));
	ThrowIfFailed(loadImage());

	constexpr bool bUseCopyTextureRegion = true;
	if (bUseCopyTextureRegion)
	{
		ThrowIfFailed(createTextureBuffer2());
	}
	else
	{
		ThrowIfFailed(createTextureBuffer());
	}

	ThrowIfFailed(createSceneMatrixBuffer());
	ThrowIfFailed(createViews());

	for (auto& actor : m_pmdActors)
	{
		actor.enableAnimation(m_bAnimationEnabled);
	}

	createPeraView();
	m_pera.createVertexBufferView();
	m_pera.compileShaders();
	m_pera.createPipelineState();

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

	// TODO: off screen buffer‚É•`‚­‚æ‚¤‚É‚·‚é
#if 0
	{
		D3D12_RESOURCE_BARRIER barrier = { };
		{
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = m_peraResource.Get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);

		const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_peraRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		Resource::instance()->getCommandList()->OMSetRenderTargets(
			1,
			&rtvH,
			false,
			nullptr); // TODO: need to set DSV view ?
	}

	{
		D3D12_RESOURCE_BARRIER barrier = { };
		{
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = m_peraResource.Get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		}
		Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);
	}
#endif

	const UINT bbIdx = Resource::instance()->getSwapChain()->GetCurrentBackBufferIndex();

	// resource barrier
	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = Resource::instance()->getBackBuffer(bbIdx);
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);


	// link swap chain's and depth buffers to output merger
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = Resource::instance()->getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * static_cast<SIZE_T>(Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

	const D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	Resource::instance()->getCommandList()->OMSetRenderTargets(1, &rtvH, false, &dsvH);

	// clear render target
	constexpr float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Resource::instance()->getCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// clear depth buffer
	Resource::instance()->getCommandList()->ClearDepthStencilView(
		dsvH,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
        nullptr);

#if 0
	// render
	for (const auto& actor : m_pmdActors)
	{
		actor.render(m_sceneDescHeap.Get());
	}
#else
	m_pera.render(m_peraSrvHeap.Get());
#endif

	// resource barrier
	{
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

HRESULT Render::loadImage()
{
	using namespace DirectX;

	auto ret = LoadFromWICFile(
		L"img/textest.png",
		DirectX::WIC_FLAGS_NONE,
		&m_metadata,
		m_scratchImage);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::createTextureBuffer()
{
	// create a heap for texture
	D3D12_HEAP_PROPERTIES heapProp = { };
	{
		heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;
	}

	D3D12_RESOURCE_DESC resDesc = {};
	{
		resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_metadata.dimension);
		resDesc.Alignment = 0;
		resDesc.Width = m_metadata.width;
		resDesc.Height = static_cast<UINT>(m_metadata.height);
		resDesc.DepthOrArraySize = static_cast<UINT16>(m_metadata.arraySize);
		resDesc.MipLevels = static_cast<UINT16>(m_metadata.mipLevels);
		resDesc.Format = m_metadata.format;
		resDesc.SampleDesc = { 1, 0 };
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(m_texResource.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	const auto img = m_scratchImage.GetImage(0, 0, 0);
	ThrowIfFalse(img != nullptr);

	// upload texture to device
	ret = m_texResource->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		static_cast<UINT>(img->rowPitch),
		static_cast<UINT>(img->slicePitch)
	);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::createTextureBuffer2()
{
	// create a resource for uploading
	const auto img = m_scratchImage.GetImage(0, 0, 0);
	ThrowIfFalse(img != nullptr);

	ComPtr<ID3D12Resource> texUploadBuff = nullptr;
	{
		D3D12_HEAP_PROPERTIES uploadHeapProp = { };
		{
			uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD; // to be able to map
			uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			uploadHeapProp.CreationNodeMask = 0;
			uploadHeapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resDesc = { };
		{

			resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resDesc.Alignment = 0;
			resDesc.Width = img->slicePitch;
			resDesc.Height = 1;
			resDesc.DepthOrArraySize = 1;
			resDesc.MipLevels = 1;
			resDesc.Format = DXGI_FORMAT_UNKNOWN; // this is chunk of data, so should be unknown
			resDesc.SampleDesc = { 1, 0 };
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&uploadHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, // CPU writable and GPU redable
			nullptr,
			IID_PPV_ARGS(texUploadBuff.ReleaseAndGetAddressOf())
		);
		ThrowIfFailed(result);
	}


	// map
	{
		uint8_t* mapForImg = nullptr;

		auto result = texUploadBuff->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mapForImg)
		);
		ThrowIfFailed(result);

		std::copy_n(
			img->pixels,
			img->slicePitch,
			mapForImg);

		texUploadBuff->Unmap(0, nullptr);
	}


	// create a resource for reading from GPU
	{
		D3D12_HEAP_PROPERTIES texHeapProp = { };
		{
			texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			texHeapProp.CreationNodeMask = 0;
			texHeapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resDesc = { };
		{
			resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_metadata.dimension);
			resDesc.Alignment = 0;
			resDesc.Width = m_metadata.width;
			resDesc.Height = static_cast<UINT>(m_metadata.height);
			resDesc.DepthOrArraySize = static_cast<UINT16>(m_metadata.arraySize);
			resDesc.MipLevels = static_cast<UINT16>(m_metadata.mipLevels);
			resDesc.Format = m_metadata.format;
			resDesc.SampleDesc = { 1, 0 };
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&texHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(m_texResource.ReleaseAndGetAddressOf())
		);
		ThrowIfFailed(result);
	}


	// copy texture
	D3D12_TEXTURE_COPY_LOCATION src = { };
	{
		src.pResource = texUploadBuff.Get();
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = img->format;
		src.PlacedFootprint.Footprint.Width = static_cast<UINT>(m_metadata.width);
		src.PlacedFootprint.Footprint.Height = static_cast<UINT>(m_metadata.height);
		src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(m_metadata.depth);
		src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(img->rowPitch);
	}
	ThrowIfFalse(src.PlacedFootprint.Footprint.RowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);
	D3D12_TEXTURE_COPY_LOCATION dst = { };
	{
		dst.pResource = m_texResource.Get();
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
	}
	Resource::instance()->getCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);


	// barrier
	D3D12_RESOURCE_BARRIER barrierDesc = { };
	{
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.Transition.pResource = m_texResource.Get();
		barrierDesc.Transition.Subresource = 0;
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	Resource::instance()->getCommandList()->ResourceBarrier(1, &barrierDesc);
	ThrowIfFailed(Resource::instance()->getCommandList()->Close());


	ID3D12CommandList* cmdLists[] = { Resource::instance()->getCommandList() };
	Resource::instance()->getCommandQueue()->ExecuteCommandLists(1, cmdLists);

	// wait until the copy is done
	ComPtr<ID3D12Fence> fence = nullptr;
	ThrowIfFailed(createFence(0, &fence));
	ThrowIfFailed(Resource::instance()->getCommandQueue()->Signal(fence.Get(), 1));

	if (fence->GetCompletedValue() != 1)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		ThrowIfFailed(fence->SetEventOnCompletion(1, event));

		auto ret = WaitForSingleObject(event, INFINITE);
		ThrowIfFalse(ret == WAIT_OBJECT_0);

		BOOL ret2 = CloseHandle(event); // fail‚·‚é

		if (!ret2)
		{
			DebugOutputFormatString("failed to close handle. (ret %d error %d)\n", ret2, GetLastError());
			ThrowIfFalse(FALSE);
		}
	}

	// reset command allocator & list
	ThrowIfFailed(Resource::instance()->getCommandAllocator()->Reset());
	ThrowIfFailed(Resource::instance()->getCommandList()->Reset(Resource::instance()->getCommandAllocator(), nullptr));

	// close it before going into main loop
	ThrowIfFailed(Resource::instance()->getCommandList()->Close());

	return S_OK;
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

		constexpr float clsClr[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
		D3D12_CLEAR_VALUE clearValue = { };
		{
			clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			memcpy(&clearValue.Color, &clsClr[0], sizeof(clearValue.Color));
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

		D3D12_SHADER_RESOURCE_VIEW_DESC resourceDesc = { };
		{
			resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			resourceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			resourceDesc.Texture2D.MostDetailedMip = 0;
			resourceDesc.Texture2D.MipLevels = 1;
			resourceDesc.Texture2D.PlaneSlice = 0;
			resourceDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Resource::instance()->getDevice()->CreateShaderResourceView(
			m_peraResource.Get(),
			&resourceDesc,
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

