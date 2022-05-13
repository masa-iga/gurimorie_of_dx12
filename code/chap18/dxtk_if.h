#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <Winerror.h>
#include <memory>
#pragma warning(pop)

class DxtkIf {
public:
	HRESULT init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);

private:
	std::unique_ptr<DirectX::GraphicsMemory> m_gfxMemory = nullptr;
	DirectX::SpriteFont* m_spriteFont = nullptr;
	DirectX::SpriteBatch* m_spriteBatch = nullptr;
};
