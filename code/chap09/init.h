#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <winerror.h>
#include <vector>
#include "debug.h"

class Resource
{
public:
	static Resource* instance()
	{
		ThrowIfFalse(s_instance != nullptr);
		return s_instance;
	}
	static void create()
	{
		if (s_instance)
			return;

		s_instance = new Resource;
	}
	static void destroy()
	{
		if (!s_instance)
			return;

		delete s_instance;
		s_instance = nullptr;
	}

	HRESULT allocate(HWND hwnd);
	HRESULT release();
	ID3D12Device* getDevice();
	ID3D12CommandAllocator* getCommandAllocator();
	ID3D12GraphicsCommandList* getCommandList();
	ID3D12CommandQueue* getCommandQueue();
	IDXGISwapChain4* getSwapChain();
	ID3D12DescriptorHeap* getRtvHeaps();
	ID3D12Resource* getBackBuffer(UINT index);

private:
	Resource() = default;

	HRESULT createDxFactory(IDXGIFactory6** ppDxgiFactory);
	HRESULT listUpAdaptors(IDXGIFactory6* pDxgiFactory);
	HRESULT createDevice(ID3D12Device** ppDevice, IUnknown* pAdapter);
	HRESULT createCommandBuffers();
	HRESULT createSwapChain(IDXGISwapChain4** ppSwapChain, IDXGIFactory6* pDxgiFactory, HWND hwnd);
	HRESULT createDescriptorHeap(ID3D12DescriptorHeap** ppRtvHeaps, std::vector<ID3D12Resource*>& backBuffers);
	HRESULT enableDebugLayer();

	static Resource* s_instance;
};
