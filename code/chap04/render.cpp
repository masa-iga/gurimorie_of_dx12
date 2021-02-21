#include "render.h"
#include <cassert>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <synchapi.h>
#include "init.h"
#include "debug.h"

#pragma comment(lib, "d3dcompiler.lib")

static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence);
static HRESULT createVertexBuffer();
static HRESULT loadShaders();
static void outputDebugMessage(ID3DBlob* errorBlob);

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));

	ThrowIfFailed(createVertexBuffer());

	ThrowIfFailed(loadShaders());

	return S_OK;
}

HRESULT Render::render()
{
	const UINT bbIdx = getInstanceOfSwapChain()->GetCurrentBackBufferIndex();

	// resource barrier
	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = getBackBuffer(bbIdx);
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	getInstanceOfCommandList()->ResourceBarrier(1, &barrier);


	// link swap chain's memory to output merger
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * static_cast<SIZE_T>(getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

	getInstanceOfCommandList()->OMSetRenderTargets(1, &rtvH, true, nullptr);


	// clear render target
	constexpr float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	getInstanceOfCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


	// resource barrier
	{
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	}
	getInstanceOfCommandList()->ResourceBarrier(1, &barrier);


	// execute command lists
	ThrowIfFailed(getInstanceOfCommandList()->Close());

	ID3D12CommandList* cmdLists[] = { getInstanceOfCommandList() };

	getInstanceOfCommandQueue()->ExecuteCommandLists(1, cmdLists);

	// send signal
	ThrowIfFailed(getInstanceOfCommandQueue()->Signal(m_pFence, ++m_fenceVal));

	return S_OK;
}

HRESULT Render::waitForEndOfRendering()
{
	while (m_pFence->GetCompletedValue() != m_fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

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

	// reset command allocator & list
	ThrowIfFailed(getInstanceOfCommandAllocator()->Reset());
	ThrowIfFailed(getInstanceOfCommandList()->Reset(getInstanceOfCommandAllocator(), nullptr));

	return S_OK;
}

HRESULT Render::swap()
{
	ThrowIfFailed(getInstanceOfSwapChain()->Present(1, 0));

	return S_OK;
}

static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence)
{
	return getInstanceOfDevice()->CreateFence(
		initVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(ppFence));
}

static HRESULT createVertexBuffer()
{
	const DirectX::XMFLOAT3 vertices[] = {
		{-1.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f},
	};

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
		resourceDesc.Width = sizeof(vertices);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	ID3D12Resource* vertBuff = nullptr;

	// create both a resrouce and an implicit heap
	auto ret = getInstanceOfDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));
	ThrowIfFailed(ret);


	// copy vertices to the allocated resource memory
	DirectX::XMFLOAT3* vertMap = nullptr;

	ret = vertBuff->Map(
		0,
		nullptr,
		(void**)&vertMap);
	ThrowIfFailed(ret);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);


	// create VB view
	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	{
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(vertices);
		vbView.StrideInBytes = sizeof(vertices[0]);
	}

	// TODO: getInstanceOfCommandList()->IASetVertexBuffers(0, 1, &vbView);

	return S_OK;
}

HRESULT loadShaders()
{
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	auto ret = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob);
	}
	ThrowIfFailed(ret);


	ret = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPs",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		&errorBlob
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob);
	}
	ThrowIfFailed(ret);

	return S_OK;
}

static void outputDebugMessage(ID3DBlob* errorBlob)
{
	if (errorBlob == nullptr)
		return;

	std::string errStr;
	errStr.resize(errorBlob->GetBufferSize());

	std::copy_n(
		static_cast<char*>(errorBlob->GetBufferPointer()),
		errorBlob->GetBufferSize(),
		errStr.begin());
	errStr += "\n";

	OutputDebugStringA(errStr.c_str());
}

