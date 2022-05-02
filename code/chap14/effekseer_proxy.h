#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Winerror.h>
#include <Effekseer.h>
#include <EffekseerRendererDX12.h>
#pragma warning(pop)

class EffekseerProxy {
public:
	HRESULT init(ID3D12Device* device, ID3D12CommandQueue* commandQueue, int32_t swapBufferCount, DXGI_FORMAT* renderTargetFormats, int32_t renderTargetCount, DXGI_FORMAT depthFormat, bool isReversedDepth, int32_t squareMaxCount);

private:
	HRESULT config();

	EffekseerRenderer::RendererRef m_efkRenderer = nullptr;
	Effekseer::ManagerRef m_efkManager = nullptr;
	Effekseer::RefPtr<EffekseerRenderer::SingleFrameMemoryPool> m_efkMemoryPool = nullptr;
	Effekseer::RefPtr<EffekseerRenderer::CommandList> m_efkCmdList = nullptr;
	Effekseer::Effect* m_effect = nullptr;
	Effekseer::Handle m_efkHandle = { };
};
