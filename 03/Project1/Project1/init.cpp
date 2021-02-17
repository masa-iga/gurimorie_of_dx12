#include "init.h"
#include <cassert>
#include <string>
#include <vector>
#include "config.h"
#include "debug.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static HRESULT createDxFactory();
static HRESULT listUpAdaptors();
static HRESULT createDevice(IUnknown *pAdapter);
static HRESULT createCommandBuffers();
static HRESULT createSwapChain(HWND hwnd);
static HRESULT createDescriptorHeap();
static HRESULT enableDebugLayer();

static constexpr UINT kNumOfSwapBuffer = 2;

static ID3D12Device* s_pDevice = nullptr;
static IDXGISwapChain4 *s_pSwapChain = nullptr;
static IDXGIFactory6* _dxgiFactory = nullptr;
static ID3D12DescriptorHeap* s_pRtvHeaps = nullptr;
static std::vector<ID3D12Resource*> s_backBuffers(kNumOfSwapBuffer);

HRESULT initGraphics(HWND hwnd)
{
	auto ret = createDxFactory();
	ThrowIfFailed(ret);

#ifdef _DEBUG
	ret = enableDebugLayer();
	ThrowIfFailed(ret);
#endif // _DEBUG

	IUnknown *adapter = nullptr;

	ret = listUpAdaptors();
	ThrowIfFailed(ret);

	ret = createDevice(adapter);
	ThrowIfFailed(ret);

	ret = createCommandBuffers();
	ThrowIfFailed(ret);

	ret = createSwapChain(hwnd);
	ThrowIfFailed(ret);

	ret = createDescriptorHeap();
	ThrowIfFailed(ret);

	return S_OK;
}

ID3D12Device* getInstanceOfDevice()
{
	assert(s_pDevice != nullptr);
	return s_pDevice;
}

ID3D12CommandAllocator* getInstanceOfCommandAllocator()
{
	static ID3D12CommandAllocator *pCommandAllocator = nullptr;

	if (pCommandAllocator != nullptr)
		return pCommandAllocator;

	auto result = getInstanceOfDevice()->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&pCommandAllocator));
	ThrowIfFailed(result);

	return pCommandAllocator;
}

ID3D12GraphicsCommandList* getInstanceOfCommandList()
{
	static ID3D12GraphicsCommandList* pCommandList = nullptr;

	if (pCommandList != nullptr)
		return pCommandList;

	auto result = getInstanceOfDevice()->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		getInstanceOfCommandAllocator(),
		nullptr,
		IID_PPV_ARGS(&pCommandList));
	ThrowIfFailed(result);

	return pCommandList;
}

ID3D12CommandQueue* getInstanceOfCommandQueue()
{
	static ID3D12CommandQueue* pCommandQueue = nullptr;

	if (pCommandQueue != nullptr)
		return pCommandQueue;

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = { };
	{
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // no time out
		cmdQueueDesc.NodeMask = 0;
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // no priority
		cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	auto result = getInstanceOfDevice()->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&pCommandQueue));
	ThrowIfFailed(result);

	return pCommandQueue;
}

IDXGISwapChain4* getInstanceOfSwapChain()
{
	assert(s_pSwapChain != nullptr);
	return s_pSwapChain;
}

ID3D12DescriptorHeap* getRtvHeaps()
{
	assert(s_pRtvHeaps != nullptr);
	return s_pRtvHeaps;
}

ID3D12Resource* getBackBuffer(UINT index)
{
	auto buffer = s_backBuffers.at(index);
	ThrowIfFalse(buffer != nullptr);
	return buffer;
}

HRESULT createDxFactory()
{
#ifdef _DEBUG
	return CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	return CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif // _DEBUG
}

HRESULT listUpAdaptors()
{
	assert(_dxgiFactory != nullptr);

	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tmpAdapter = nullptr;

	for (int32_t i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	DebugOutputFormatString("Available adapters:\n");

	for (const auto& adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = { };
		adpt->GetDesc(&adesc);

		const std::wstring &strDesc = adesc.Description;

		DebugOutputFormatString(" - %ls\n", strDesc.c_str());
	}
	DebugOutputFormatString("\n");

	return S_OK;
}

HRESULT createDevice(IUnknown *pAdapter)
{
	constexpr D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_1_0_CORE;

	for (const auto &lv : levels)
	{
		auto ret = D3D12CreateDevice(pAdapter, lv, IID_PPV_ARGS(&s_pDevice));

		if (SUCCEEDED(ret))
		{
			featureLevel = lv;
			break;
		}
	}

	DebugOutputFormatString("D3D feature level: 0x%x\n", featureLevel);

	return S_OK;
}

HRESULT createCommandBuffers()
{
	[[maybe_unused]] const auto commandAllocator = getInstanceOfCommandAllocator();
	assert(commandAllocator != nullptr);

	[[maybe_unused]] const auto commandList = getInstanceOfCommandList();
	assert(commandList != nullptr);

	[[maybe_unused]] const auto commandQueue = getInstanceOfCommandQueue();
	assert(commandQueue != nullptr);

	return S_OK;
}

HRESULT createSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = { };
	{
		swapchainDesc.Width = kWindowWidth;
		swapchainDesc.Height = kWindowHeight;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.Stereo = false;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
		swapchainDesc.BufferCount = kNumOfSwapBuffer;
		swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	}

	auto result = _dxgiFactory->CreateSwapChainForHwnd(
		getInstanceOfCommandQueue(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&s_pSwapChain
	);

	return result;
}

HRESULT createDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
	{
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NodeMask = 0;
		heapDesc.NumDescriptors = kNumOfSwapBuffer;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}

	auto result = getInstanceOfDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s_pRtvHeaps));
	ThrowIfFailed(result);

	DXGI_SWAP_CHAIN_DESC swcDesc = { };
	result = getInstanceOfSwapChain()->GetDesc(&swcDesc);
	ThrowIfFailed(result);

	for (uint32_t i = 0; i < swcDesc.BufferCount; ++i)
	{
		result = getInstanceOfSwapChain()->GetBuffer(i, IID_PPV_ARGS(&s_backBuffers[i]));
		ThrowIfFailed(result);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += i * static_cast<SIZE_T>(getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

		getInstanceOfDevice()->CreateRenderTargetView(
			s_backBuffers[i],
			nullptr,
			handle);
	}

	return S_OK;
}

static HRESULT enableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;

	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	ThrowIfFailed(result);

	debugLayer->EnableDebugLayer();
	debugLayer->Release();

	return S_OK;
}
