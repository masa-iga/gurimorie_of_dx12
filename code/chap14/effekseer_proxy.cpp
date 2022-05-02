#include "effekseer_proxy.h"
#include "debug.h"

#if NDEBUG
#pragma comment(lib, "EffekseerRendererDX12.lib")
#pragma comment(lib, "Effekseer.lib")
#pragma comment(lib, "LLGI.lib")
#else
#pragma comment(lib, "EffekseerRendererDX12d.lib")
#pragma comment(lib, "Effekseerd.lib")
#pragma comment(lib, "LLGId.lib")
#endif // NDEBUG

HRESULT EffekseerProxy::init(ID3D12Device* device, ID3D12CommandQueue* commandQueue, int32_t swapBufferCount, DXGI_FORMAT* renderTargetFormats, int32_t renderTargetCount, DXGI_FORMAT depthFormat, bool isReversedDepth, int32_t squareMaxCount)
{
	m_efkRendererRef = EffekseerRendererDX12::Create(device, commandQueue, swapBufferCount, renderTargetFormats, renderTargetCount, depthFormat, isReversedDepth, squareMaxCount);
	ThrowIfFalse(m_efkRendererRef != nullptr);

	return S_OK;
}
