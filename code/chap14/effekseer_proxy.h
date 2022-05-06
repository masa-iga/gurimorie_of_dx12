#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <Winerror.h>
#include <Effekseer.h>
#include <EffekseerRendererDX12.h>
#pragma warning(pop)

class EffekseerProxy {
public:
	HRESULT init(ID3D12Device* device, ID3D12CommandQueue* commandQueue, int32_t swapBufferCount, DXGI_FORMAT* renderTargetFormats, int32_t renderTargetCount, DXGI_FORMAT depthFormat, bool isReversedDepth, int32_t squareMaxCount);
	HRESULT load();
	HRESULT play();
	HRESULT update();
	void setCameraMatrix(const DirectX::XMMATRIX& mat);
	void setProjectionMatrix(const DirectX::XMMATRIX& mat);
	HRESULT draw(ID3D12GraphicsCommandList* cmdList);

private:
	inline static const char16_t kEffectPath[] = u"../effekseer/resource/10/SimpleLaser.efk";
	inline static const char16_t kMaterialPath[] = u"../effekseer/resource/10";

	HRESULT config();

	EffekseerRenderer::RendererRef m_efkRenderer = nullptr;
	Effekseer::ManagerRef m_efkManager = nullptr;
	Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> m_efkMemoryPool = nullptr;
	Effekseer::RefPtr<EffekseerRenderer::CommandList> m_efkCmdList = nullptr;
	Effekseer::RefPtr<Effekseer::Effect> m_effect = nullptr;
	Effekseer::Handle m_efkHandle = { };
};
