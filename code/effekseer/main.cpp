#include <Effekseer.h>
#include <EffekseerRendererDX12.h>

#pragma comment(lib, "EffekseerRendererDX12.lib")
#pragma comment(lib, "Effekseer.lib")
#pragma comment(lib, "LLGI.lib")

void init()
{
	EffekseerRenderer::Renderer* m_efkRenderer = nullptr;
	Effekseer::Manager* m_efkManager = nullptr;

	EffekseerRenderer::SingleFrameMemoryPool* m_efkMemoryPool = nullptr;

	EffekseerRenderer::CommandList* m_efkCmdList = nullptr;

	Effekseer::Effect* m_effect = nullptr;

	Effekseer::Handle m_efkHandle;
}