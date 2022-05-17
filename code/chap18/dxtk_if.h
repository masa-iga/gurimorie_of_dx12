#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <GraphicsMemory.h>
#include <ResourceUploadBatch.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <Winerror.h>
#include <functional>
#include <memory>
#include <wrl.h>
#pragma warning(pop)

class DxtkIf {
public:
	HRESULT init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat);
	HRESULT upload(ID3D12CommandQueue* queue, std::function<HRESULT(ID3D12CommandQueue* queue)> callbackWaitForCompletion);
	HRESULT draw(ID3D12GraphicsCommandList* list);
	HRESULT commit(ID3D12CommandQueue* queue);
	void setViewport(const D3D12_VIEWPORT& viewPort);

private:
	const wchar_t* kFilePath = L"../resource/dxtk/fonttest.spritefont";

	std::unique_ptr<DirectX::GraphicsMemory> m_gfxMemory = nullptr;
	std::unique_ptr<DirectX::SpriteFont> m_spriteFont = nullptr;
	std::unique_ptr<DirectX::SpriteBatch> m_spriteBatch = nullptr;
	std::unique_ptr<DirectX::ResourceUploadBatch> m_resourceUploadBatch = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap = nullptr;
};
