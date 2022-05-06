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

HRESULT EffekseerProxy::play()
{
	const Effekseer::Vector3D initPos(0.0f, 0.0f, 0.0f);

	m_efkHandle = m_efkManager->Play(m_effect, initPos);

	return S_OK;
}

HRESULT EffekseerProxy::update()
{
	m_efkManager->Update();
	m_efkMemoryPool->NewFrame();
	return S_OK;
}

void EffekseerProxy::setCameraMatrix(const DirectX::XMMATRIX& mat)
{
	Effekseer::Matrix44 m;

	for (uint32_t i = 0; i < 4; ++i)
	{
		for (uint32_t j = 0; j < 4; ++j)
		{
			m.Values[i][j] = mat.r[i].m128_f32[j];
		}
	}

	m_efkRenderer->SetCameraMatrix(m);
}

void EffekseerProxy::setProjectionMatrix(const DirectX::XMMATRIX& mat)
{
	Effekseer::Matrix44 m;

	for (uint32_t i = 0; i < 4; ++i)
	{
		for (uint32_t j = 0; j < 4; ++j)
		{
			m.Values[i][j] = mat.r[i].m128_f32[j];
		}
	}

	m_efkRenderer->SetProjectionMatrix(m);
}

HRESULT EffekseerProxy::draw(ID3D12GraphicsCommandList* cmdList)
{
	EffekseerRendererDX12::BeginCommandList(m_efkCmdList, cmdList);
	m_efkRenderer->SetCommandList(m_efkCmdList);

	m_efkRenderer->BeginRendering();
	m_efkManager->Draw();
	m_efkRenderer->EndRendering();

	m_efkRenderer->SetCommandList(nullptr);
	EffekseerRendererDX12::EndCommandList(m_efkCmdList);

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
	m_efkManager->SetMaterialLoader(m_efkRenderer->CreateMaterialLoader());
	m_efkManager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

	m_efkMemoryPool = EffekseerRenderer::CreateSingleFrameMemoryPool(m_efkRenderer->GetGraphicsDevice());
	ThrowIfFalse(m_efkMemoryPool != nullptr);

	m_efkCmdList = EffekseerRenderer::CreateCommandList(m_efkRenderer->GetGraphicsDevice(), m_efkMemoryPool);
	ThrowIfFalse(m_efkCmdList != nullptr);

	return S_OK;
}
