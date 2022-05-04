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
	m_efkRenderer = EffekseerRendererDX12::Create(device, commandQueue, swapBufferCount, renderTargetFormats, renderTargetCount, depthFormat, isReversedDepth, squareMaxCount);
	ThrowIfFalse(m_efkRenderer != nullptr);

	const int32_t instanceMax = squareMaxCount;
	m_efkManager = Effekseer::Manager::Create(instanceMax);
	ThrowIfFalse(m_efkManager != nullptr);

	ThrowIfFailed(config());

	return S_OK;
}

HRESULT EffekseerProxy::load()
{
	m_effect = Effekseer::Effect::Create(
		m_efkManager,
		kEffectPath,
		1.0f,
		kMaterialPath);
	ThrowIfFalse(m_effect != nullptr);

	return S_OK;
}

HRESULT EffekseerProxy::config()
{
	m_efkManager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);

	m_efkManager->SetSpriteRenderer(m_efkRenderer->CreateSpriteRenderer());
	m_efkManager->SetRibbonRenderer(m_efkRenderer->CreateRibbonRenderer());
	m_efkManager->SetRingRenderer(m_efkRenderer->CreateRingRenderer());
	m_efkManager->SetTrackRenderer(m_efkRenderer->CreateTrackRenderer());
	m_efkManager->SetModelRenderer(m_efkRenderer->CreateModelRenderer());

	m_efkManager->SetTextureLoader(m_efkRenderer->CreateTextureLoader());
	m_efkManager->SetModelLoader(m_efkRenderer->CreateModelLoader());

	m_efkMemoryPool = EffekseerRenderer::CreateSingleFrameMemoryPool(m_efkRenderer->GetGraphicsDevice());
	ThrowIfFalse(m_efkMemoryPool != nullptr);

	m_efkCmdList = EffekseerRenderer::CreateCommandList(m_efkRenderer->GetGraphicsDevice(), m_efkMemoryPool);
	ThrowIfFalse(m_efkCmdList != nullptr);

	m_efkRenderer->SetCommandList(m_efkCmdList);

	return S_OK;
}
