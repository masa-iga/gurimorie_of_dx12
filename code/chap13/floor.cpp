#include "floor.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

HRESULT Floor::init()
{
	ThrowIfFailed(createVertexResource());

	// set up shaders

	// set up graphics pipeline

	// set up root signature

	return S_OK;
}

HRESULT Floor::renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap)
{
	return S_OK;
}

HRESULT Floor::render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);

	setInputAssembler(list);
	setRasterizer(list);

	// set graphics pipeline
	// set root signature
	// bind descriptor heap to root signature

	//list->DrawInstanced(6, 1, 0, 0);

	return S_OK;
}

HRESULT Floor::createVertexResource()
{
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
	};

	constexpr Vertex kVertices[] =
	{
		DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f),
		DirectX::XMFLOAT3(-1.0f,  1.0,  0.0f),
		DirectX::XMFLOAT3( 1.0f,  1.0,  0.0f),
		DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f),
		DirectX::XMFLOAT3( 1.0f,  1.0f, 0.0f),
		DirectX::XMFLOAT3( 1.0f, -1.0f, 0.0f),
	};

	constexpr size_t bufferSize = sizeof(Vertex) * kNumOfVertices;
	const auto w = static_cast<uint32_t>(Util::alignmentedSize(bufferSize, 256));

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
			resourceDesc.Width = 256;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1 , 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_vertResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		m_vbView.BufferLocation = m_vertResource.Get()->GetGPUVirtualAddress();
		m_vbView.SizeInBytes = w;
		m_vbView.StrideInBytes = sizeof(Vertex);
	}

	{
		Vertex* pVertices = nullptr;

		auto result = m_vertResource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
		ThrowIfFailed(result);

		std::copy(std::begin(kVertices), std::end(kVertices), pVertices);

		m_vertResource.Get()->Unmap(0, nullptr);
	}

	return S_OK;
}

void Floor::setInputAssembler(ID3D12GraphicsCommandList* list) const
{
	ThrowIfFalse(list != nullptr);

	list->IASetPrimitiveTopology(kPrimTopology);
	list->IASetVertexBuffers(0, 1, &m_vbView);
}

void Floor::setRasterizer(ID3D12GraphicsCommandList* list) const
{
	{
		D3D12_VIEWPORT viewport = { };
		{
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = static_cast<float>(kWindowWidth);
			viewport.Height = static_cast<float>(kWindowHeight);
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 0.0f;
		}
		list->RSSetViewports(1, &viewport);
	}

	{
		D3D12_RECT scissorRect = { };
		{
			scissorRect.left = 0;
			scissorRect.top = 0;
			scissorRect.right = scissorRect.left + kWindowWidth;
			scissorRect.bottom = scissorRect.right + kWindowHeight;
		}
		list->RSSetScissorRects(1, &scissorRect);
	}

}
