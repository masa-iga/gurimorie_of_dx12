#include <cassert>
#include "init.h"
#include "render.h"
#include "debug.h"

HRESULT render()
{
	// link to swap chain's memory
	const UINT bbIdx = getSwapChainInstance()->GetCurrentBackBufferIndex();

	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * getDeviceInstance()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	getCommandListInstance()->OMSetRenderTargets(1, &rtvH, true, nullptr);


	// clear render target
	constexpr float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	getCommandListInstance()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


	// execute command lists
	ThrowIfFailed(getCommandListInstance()->Close());

	ID3D12CommandList* cmdLists[] = { getCommandListInstance() };

	getCommandQueueInstance()->ExecuteCommandLists(1, cmdLists);


	// reset command allocator & list
	ThrowIfFailed(getCommandAllocatorInstance()->Reset());
	ThrowIfFailed(getCommandListInstance()->Reset(getCommandAllocatorInstance(), nullptr));


	// swap
	ThrowIfFailed(getSwapChainInstance()->Present(1, 0));

	return S_OK;
}