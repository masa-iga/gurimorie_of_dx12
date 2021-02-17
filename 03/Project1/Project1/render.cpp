#include "render.h"
#include <cassert>
#include <synchapi.h>
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