#include "dxtk_if.h"
#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dx12.h>
#include <SpriteFont.h>
#pragma warning(pop)
#include "debug.h"

#pragma comment(lib, "DirectXTK12.lib")

HRESULT DxtkIf::init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat)
{
	m_gfxMemory = std::make_unique<DirectX::GraphicsMemory>(DirectX::GraphicsMemory(device));

	DirectX::ResourceUploadBatch resourceUploadBatch(device);
	resourceUploadBatch.Begin();

	DirectX::RenderTargetState rsState(rtFormat, dsFormat);
	DirectX::SpriteBatchPipelineStateDescription pd(rsState);
	m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(DirectX::SpriteBatch(device, resourceUploadBatch, pd));

	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDsc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0,
		};

		auto result = device->CreateDescriptorHeap(&heapDsc, IID_PPV_ARGS(m_descHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	m_spriteFont = std::make_unique<DirectX::SpriteFont>(DirectX::SpriteFont(
		device,
		resourceUploadBatch,
		kFilePath,
		m_descHeap.Get()->GetCPUDescriptorHandleForHeapStart(),
		m_descHeap.Get()->GetGPUDescriptorHandleForHeapStart()));
	ThrowIfFalse(m_spriteFont != nullptr);

	m_resourceUploadBatch = std::make_unique<DirectX::ResourceUploadBatch>(std::move(resourceUploadBatch));

	return S_OK;
}

HRESULT DxtkIf::upload(ID3D12CommandQueue* queue, std::function<HRESULT(ID3D12CommandQueue* queue)> callbackWaitForCompletion)
{
	ThrowIfFalse(queue != nullptr);

	auto future = m_resourceUploadBatch.get()->End(queue);

	// wait until command queue is processed
	callbackWaitForCompletion(queue);
	future.wait();

	return S_OK;
}

HRESULT DxtkIf::draw(ID3D12GraphicsCommandList* list)
{
	ThrowIfFalse(list != nullptr);

	list->SetDescriptorHeaps(1, m_descHeap.GetAddressOf());

	m_spriteBatch->Begin(list);
	{
		m_spriteFont->DrawString(
			m_spriteBatch.get(),
			"Hello World",
			DirectX::XMFLOAT2(102, 102),
			DirectX::Colors::Black);

		m_spriteFont->DrawString(
			m_spriteBatch.get(),
			"Hello World",
			DirectX::XMFLOAT2(100, 100),
			DirectX::Colors::Yellow);
	}
	m_spriteBatch->End();

	return S_OK;
}

HRESULT DxtkIf::commit(ID3D12CommandQueue* queue)
{
	m_gfxMemory->Commit(queue);
	return S_OK;
}

void DxtkIf::setViewport(const D3D12_VIEWPORT& viewPort)
{
	m_spriteBatch.get()->SetViewport(viewPort);
}

