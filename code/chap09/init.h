#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <winerror.h>

HRESULT initGraphics(HWND hwnd);
HRESULT close();
ID3D12Device* getInstanceOfDevice();
ID3D12CommandAllocator* getInstanceOfCommandAllocator();
ID3D12GraphicsCommandList* getInstanceOfCommandList();
ID3D12CommandQueue* getInstanceOfCommandQueue();
IDXGISwapChain4* getInstanceOfSwapChain();
ID3D12DescriptorHeap* getRtvHeaps();
ID3D12Resource* getBackBuffer(UINT index);
