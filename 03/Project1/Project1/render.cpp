#include <cassert>
#include "init.h"
#include "render.h"
#include "debug.h"

HRESULT render()
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


	// reset command allocator & list
	ThrowIfFailed(getInstanceOfCommandAllocator()->Reset());
	ThrowIfFailed(getInstanceOfCommandList()->Reset(getInstanceOfCommandAllocator(), nullptr));


	// swap
	ThrowIfFailed(getInstanceOfSwapChain()->Present(1, 0));

	return S_OK;
}