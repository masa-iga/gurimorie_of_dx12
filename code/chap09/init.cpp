#include "init.h"
#include <cassert>
#include <string>
#include "config.h"
#include "util.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static constexpr UINT kNumOfSwapBuffer = 2;

static IDXGIFactory6* s_pDxgiFactory = nullptr;
static ID3D12Device* s_pDevice = nullptr;
static ID3D12CommandAllocator* s_pCommandAllocator = nullptr;
static ID3D12GraphicsCommandList* s_pCommandList = nullptr;
static ID3D12CommandQueue* s_pCommandQueue = nullptr;
static IDXGISwapChain4* s_pSwapChain = nullptr;
static ID3D12DescriptorHeap* s_pRtvHeaps = nullptr;
static std::vector<ID3D12Resource*> s_backBuffers(kNumOfSwapBuffer, nullptr);
Resource* Resource::s_instance = nullptr;

HRESULT Resource::allocate(HWND hwnd)
{
	Util::init();

	auto ret = createDxFactory(&s_pDxgiFactory);
	ThrowIfFailed(ret);

#ifdef _DEBUG
	ret = enableDebugLayer();
	ThrowIfFailed(ret);
#endif // _DEBUG

	IUnknown *adapter = nullptr;

	ret = listUpAdaptors(s_pDxgiFactory);
	ThrowIfFailed(ret);

	ret = createDevice(&s_pDevice, adapter);
	ThrowIfFailed(ret);

	ret = createCommandBuffers();
	ThrowIfFailed(ret);

	ret = createSwapChain(&s_pSwapChain, s_pDxgiFactory, hwnd);
	ThrowIfFailed(ret);

	ret = createDescriptorHeap(&s_pRtvHeaps, s_backBuffers);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Resource::release()
{
	s_pRtvHeaps->Release();
	s_pSwapChain->Release();

	s_pCommandAllocator->Release();
	s_pCommandList->Release();
	s_pCommandQueue->Release();

	// TODO: unbind display buffer before release
//	for (auto& buffer : s_backBuffers)
//	{
//		buffer->Release();
//	}

	return S_OK;
}

ID3D12Device* Resource::getDevice()
{
	assert(s_pDevice != nullptr);
	return s_pDevice;
}

ID3D12CommandAllocator* Resource::getCommandAllocator()
{
	if (s_pCommandAllocator != nullptr)
		return s_pCommandAllocator;

	auto result = getDevice()->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&s_pCommandAllocator));
	ThrowIfFailed(result);

	return s_pCommandAllocator;
}

ID3D12GraphicsCommandList* Resource::getCommandList()
{
	if (s_pCommandList != nullptr)
		return s_pCommandList;

	auto result = getDevice()->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		getCommandAllocator(),
		nullptr,
		IID_PPV_ARGS(&s_pCommandList));
	ThrowIfFailed(result);

	return s_pCommandList;
}

ID3D12CommandQueue* Resource::getCommandQueue()
{
	if (s_pCommandQueue != nullptr)
		return s_pCommandQueue;

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = { };
	{
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // no time out
		cmdQueueDesc.NodeMask = 0;
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // no priority
		cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	auto result = getDevice()->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&s_pCommandQueue));
	ThrowIfFailed(result);

	return s_pCommandQueue;
}

IDXGISwapChain4* Resource::getSwapChain()
{
	assert(s_pSwapChain != nullptr);
	return s_pSwapChain;
}

ID3D12DescriptorHeap* Resource::getRtvHeaps()
{
	assert(s_pRtvHeaps != nullptr);
	return s_pRtvHeaps;
}

ID3D12Resource* Resource::getBackBuffer(UINT index)
{
	auto buffer = s_backBuffers.at(index);
	ThrowIfFalse(buffer != nullptr);
	return buffer;
}

HRESULT Resource::createDxFactory(IDXGIFactory6** ppDxgiFactory)
{
#ifdef _DEBUG
	return CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(ppDxgiFactory));
#else
	return CreateDXGIFactory1(IID_PPV_ARGS(ppDxgiFactory));
#endif // _DEBUG
}

HRESULT Resource::listUpAdaptors(IDXGIFactory6* pDxgiFactory)
{
	assert(pDxgiFactory != nullptr);

	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tmpAdapter = nullptr;

	for (int32_t i = 0; pDxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
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

HRESULT Resource::createDevice(ID3D12Device** ppDevice, IUnknown* pAdapter)
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
		auto ret = D3D12CreateDevice(pAdapter, lv, IID_PPV_ARGS(ppDevice));

		if (SUCCEEDED(ret))
		{
			featureLevel = lv;
			break;
		}
	}

	DebugOutputFormatString("D3D feature level: 0x%x\n", featureLevel);

	return S_OK;
}

HRESULT Resource::createCommandBuffers()
{
	[[maybe_unused]] const auto commandAllocator = getCommandAllocator();
	assert(commandAllocator != nullptr);

	[[maybe_unused]] const auto commandList = getCommandList();
	assert(commandList != nullptr);

	[[maybe_unused]] const auto commandQueue = getCommandQueue();
	assert(commandQueue != nullptr);

	return S_OK;
}

HRESULT Resource::createSwapChain(IDXGISwapChain4** ppSwapChain, IDXGIFactory6* pDxgiFactory, HWND hwnd)
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

	auto result = pDxgiFactory->CreateSwapChainForHwnd(
		getCommandQueue(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(ppSwapChain));

	return result;
}

HRESULT Resource::createDescriptorHeap(ID3D12DescriptorHeap** ppRtvHeaps, std::vector<ID3D12Resource*>& backBuffers)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
	{
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		heapDesc.NodeMask = 0;
		heapDesc.NumDescriptors = kNumOfSwapBuffer;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}

	auto result = getDevice()->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(ppRtvHeaps));
	ThrowIfFailed(result);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
	{
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = { };
	result = getSwapChain()->GetDesc(&swcDesc);
	ThrowIfFailed(result);

	for (uint32_t i = 0; i < swcDesc.BufferCount; ++i)
	{
		result = getSwapChain()->GetBuffer(
			i,
			IID_PPV_ARGS(&backBuffers[i]));
		ThrowIfFailed(result);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += i * static_cast<SIZE_T>(getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

		getDevice()->CreateRenderTargetView(
			backBuffers[i],
			&rtvDesc,
			handle);
	}

	return S_OK;
}

HRESULT Resource::enableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;

	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	ThrowIfFailed(result);

	debugLayer->EnableDebugLayer();
	debugLayer->Release();

	return S_OK;
}

