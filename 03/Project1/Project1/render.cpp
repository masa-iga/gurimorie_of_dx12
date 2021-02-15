#include "render.h"
#include <cassert>
#include "init.h"
#include "debug.h"

static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence);

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));

	return S_OK;
}

HRESULT Render::render()
{
	// link to swap chain's memory
	const UINT bbIdx = getInstanceOfSwapChain()->GetCurrentBackBufferIndex();

	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	getInstanceOfCommandList()->OMSetRenderTargets(1, &rtvH, true, nullptr);


	// clear render target
	constexpr float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	getInstanceOfCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


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
		;
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