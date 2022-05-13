#include <SpriteFont.h>
#include <ResourceUploadBatch.h>
#include "dxtk_if.h"

#pragma comment(lib, "DirectXTK12.lib")

HRESULT DxtkIf::init(ID3D12Device* device, DXGI_FORMAT rtFormat, DXGI_FORMAT dsFormat)
{
	m_gfxMemory = std::make_unique<DirectX::GraphicsMemory>(DirectX::GraphicsMemory(device));

	DirectX::ResourceUploadBatch resourceUploadBatch(device);
	resourceUploadBatch.Begin();

	DirectX::RenderTargetState rsState(rtFormat, dsFormat);

	return S_OK;
}
