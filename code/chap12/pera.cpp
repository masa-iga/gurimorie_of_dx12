#include "pera.h"
#include "debug.h"
#include "init.h"

using namespace Microsoft::WRL;

HRESULT Pera::createView()
{
	// create resource for render-to-texture
	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		constexpr float clsClr[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
		D3D12_CLEAR_VALUE clearValue = { };
		{
			clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			memcpy(&clearValue.Color, &clsClr[0], sizeof(clearValue.Color));
		}

		D3D12_RESOURCE_DESC resDesc = Resource::instance()->getBackBuffer(0)->GetDesc();

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&clearValue,
			IID_PPV_ARGS(m_resource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	// create RTV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = Resource::instance()->getRtvHeaps()->GetDesc();
		{
			heapDesc.NumDescriptors = 1;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_rtvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = { };
		{
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		}

		Resource::instance()->getDevice()->CreateRenderTargetView(
			m_resource.Get(),
			&rtvDesc,
			m_rtvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	// create SRV heap
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_srvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		D3D12_SHADER_RESOURCE_VIEW_DESC resourceDesc = { };
		{
			resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			resourceDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			resourceDesc.Texture2D.MostDetailedMip = 0;
			resourceDesc.Texture2D.MipLevels = 1;
			resourceDesc.Texture2D.PlaneSlice = 0;
			resourceDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Resource::instance()->getDevice()->CreateShaderResourceView(
			m_resource.Get(),
			&resourceDesc,
			m_srvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
}

HRESULT Pera::createTexture()
{
	struct PeraVertex
	{
		DirectX::XMFLOAT3 pos = { };
		DirectX::XMFLOAT2 uv = { };
	};

	constexpr PeraVertex pv[4] = {
		{{ -1.0f, -1.0f, 0.1f}, {0.0f, 1.0f}}, // left-lower
		{{ -1.0f,  1.0f, 0.1f}, {0.0f, 0.0f}}, // left-upper
		{{  1.0f, -1.0f, 0.1f}, {1.0f, 1.0f}}, // right-lower
		{{  1.0f,  1.0f, 0.1f}, {1.0f, 0.0f}}, // right-upper
	};

	ComPtr<ID3D12Resource> peraVB = { };

	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = sizeof(pv);
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(peraVB.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		PeraVertex* pMappedPera = nullptr;

		auto result = peraVB->Map(0, nullptr, reinterpret_cast<void**>(&pMappedPera));
		ThrowIfFailed(result);

		std::copy(std::begin(pv), std::end(pv), pMappedPera);

		peraVB->Unmap(0, nullptr);
	}

	D3D12_VERTEX_BUFFER_VIEW peraVBV = { };
	{
		peraVBV.BufferLocation = peraVB.Get()->GetGPUVirtualAddress();
		peraVBV.SizeInBytes = sizeof(pv);
		peraVBV.StrideInBytes = sizeof(PeraVertex);
	}

	return S_OK;
}

HRESULT Pera::render()
{
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvH = m_rtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();

	{
		D3D12_RESOURCE_BARRIER barrier = { };
		{
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = m_resource.Get();
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		}
		Resource::instance()->getCommandList()->ResourceBarrier(1, &barrier);
	}

	return S_OK;
}
