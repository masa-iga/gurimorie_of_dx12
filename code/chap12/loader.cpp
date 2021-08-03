#include "loader.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXTex.h>
#pragma warning(pop)
#include "init.h"
#include "util.h"

Loader* Loader::m_loader = nullptr;

using namespace Microsoft::WRL;

HRESULT Loader::loadImageFromFile(const std::string& texPath, ComPtr<ID3D12Resource>& buffer)
{
	const auto it = m_resourceTable.find(texPath);

	if (it != m_resourceTable.end())
	{
		buffer = it->second;
		return S_OK;
	}

	using namespace DirectX;

	TexMetadata metadata = { };
	ScratchImage scratchImg = { };

	auto ret = LoadFromWICFile(
		Util::getWideStringFromString(texPath).c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg);

	if (FAILED(ret))
		return S_FALSE;

	const auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = { };
	{
		heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;
	}

	D3D12_RESOURCE_DESC resourceDesc = { };
	{
		resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		resourceDesc.Alignment = 0;
		resourceDesc.Width = metadata.width;
		resourceDesc.Height = static_cast<UINT>(metadata.height);
		resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		resourceDesc.Format = metadata.format;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	ComPtr<ID3D12Resource> resource = nullptr;
	{
		ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);

		ret = resource->WriteToSubresource(
			0,
			nullptr,
			img->pixels,
			static_cast<UINT>(img->rowPitch),
			static_cast<UINT>(img->slicePitch));
		ThrowIfFailed(ret);
	}

	m_resourceTable[texPath] = resource.Get();

	buffer = resource;
	return S_OK;
}