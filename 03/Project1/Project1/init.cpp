#include "init.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <string>
#include <vector>
#include "debug.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

static HRESULT listUpAdaptors();
static HRESULT createDevice(IUnknown *pAdapter);

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

HRESULT initGraphics()
{
	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
	assert(result == S_OK);

	IUnknown *adapter = nullptr;

	auto ret = listUpAdaptors();
	assert(ret == S_OK);

	ret = createDevice(adapter);
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

