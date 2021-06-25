#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <Windows.h>
#include <winerror.h>
#include <vector>
#include <wrl.h>
#include "debug.h"

class Resource
{
public:
	static Resource* instance()
	{
		ThrowIfFalse(m_instance != nullptr);
		return m_instance;
	}
	static void create()
	{
		if (m_instance)
			return;

		m_instance = new Resource;
	}
	static void destroy()
	{
		if (!m_instance)
			return;

		delete m_instance;
		m_instance = nullptr;
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
	Resource(const Resource&) = delete;
	void operator=(const Resource&) = delete;

	HRESULT createDxFactory(Microsoft::WRL::ComPtr<IDXGIFactory6>* dxgiFactory);
	HRESULT listUpAdaptors(IDXGIFactory6* pDxgiFactory);
	HRESULT createDevice(Microsoft::WRL::ComPtr<ID3D12Device>* device, IUnknown* pAdapter);
	HRESULT createCommandBuffers();
	HRESULT createSwapChain(IDXGISwapChain4** ppSwapChain, IDXGIFactory6* pDxgiFactory, HWND hwnd);
	HRESULT createDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>* rtvHeaps, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& backBuffers);
	HRESULT enableDebugLayer();

	static Resource* m_instance;
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_pDxgiFactory = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator = nullptr;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList = nullptr;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRtvHeaps = nullptr;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_backBuffers;
};
