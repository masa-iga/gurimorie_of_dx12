#include "init.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <string>
#include <vector>
#include "config.h"
#include "debug.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static HRESULT listUpAdaptors();
static HRESULT createDevice(IUnknown *pAdapter);
static HRESULT createCommandBuffers();
static HRESULT createSwapChain(HWND hwnd);

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;

HRESULT initGraphics(HWND hwnd)
{
	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
	assert(result == S_OK);

	IUnknown *adapter = nullptr;

	auto ret = listUpAdaptors();
	assert(ret == S_OK);

	ret = createDevice(adapter);
	assert(ret == S_OK);

	ret = createCommandBuffers();
	assert(ret == S_OK);

	ret = createSwapChain(hwnd);
	assert(ret == S_OK);

	return S_OK;
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
		auto ret = D3D12CreateDevice(pAdapter, lv, IID_PPV_ARGS(&_dev));

		if (FAILED(ret))
			continue;

		featureLevel = lv;
		break;
	}

	DebugOutputFormatString("D3D feature level: 0x%x\n", featureLevel);

	return S_OK;
}

HRESULT createCommandBuffers()
{
	auto result = _dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&_cmdAllocator));
	assert(result == S_OK);

	result = _dev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		_cmdAllocator,
		nullptr,
		IID_PPV_ARGS(&_cmdList));
	assert(result == S_OK);

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = { };
	{
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // no time out
		cmdQueueDesc.NodeMask = 0;
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // no priority
		cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	}

	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));
	assert(result == S_OK);

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
		swapchainDesc.BufferCount = 2;
		swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	}

	auto result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain
	);

	return S_OK;
}
