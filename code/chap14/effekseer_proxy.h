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
	HRESULT init();

private:
	EffekseerRenderer::Renderer* m_efkRenderer = nullptr;
	Effekseer::Manager* m_efkManager = nullptr;
	EffekseerRenderer::SingleFrameMemoryPool* m_efkMemoryPool = nullptr;
	EffekseerRenderer::CommandList* m_efkCmdList = nullptr;
	Effekseer::Effect* m_effect = nullptr;
	Effekseer::Handle m_efkHandle = { };
};
