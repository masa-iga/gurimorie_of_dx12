#include "dxtk_if.h"
#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dx12.h>
#include <SpriteFont.h>
#include <ResourceUploadBatch.h>
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

	return S_OK;
}
