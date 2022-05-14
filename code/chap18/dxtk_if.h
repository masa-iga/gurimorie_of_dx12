#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <GraphicsMemory.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <Winerror.h>
#include <memory>
#include <wrl.h>
#pragma warning(pop)

class DxtkIf {
public:
	HRESULT init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);

private:
	const wchar_t* kFilePath = L"../resource/dxtk/fonttest.spritefont";

	std::unique_ptr<DirectX::GraphicsMemory> m_gfxMemory = nullptr;
	std::unique_ptr<DirectX::SpriteFont> m_spriteFont = nullptr;
	std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap = nullptr;
};
