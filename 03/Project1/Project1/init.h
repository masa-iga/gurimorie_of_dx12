#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <winerror.h>

HRESULT initGraphics(HWND hwnd);
ID3D12Device* getDeviceInstance();
ID3D12CommandAllocator* getCommandAllocatorInstance();
ID3D12GraphicsCommandList* getCommandListInstance();
ID3D12CommandQueue* getCommandQueueInstance();
IDXGISwapChain4* getSwapChainInstance();
ID3D12DescriptorHeap* getRtvHeaps();
