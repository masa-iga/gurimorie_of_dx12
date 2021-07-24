#include "pera.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

static std::vector<float> getGaussianWeights(size_t count, float sigma);

HRESULT Pera::createResources()
{
	ThrowIfFailed(createVertexBufferResource());
	ThrowIfFailed(createBokehResource());

	return S_OK;
}

HRESULT Pera::compileShaders()
{
	ComPtr<ID3DBlob> errBlob = nullptr;

	auto result = D3DCompileFromFile(
		L"peraVertex.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		0,
		0,
		m_vs.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errBlob.Get());
	}
	ThrowIfFailed(result);

	result = D3DCompileFromFile(
		L"peraPixel.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		0,
		0,
		m_ps.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errBlob.Get());
	}
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Pera::createPipelineState()
{
	constexpr D3D12_INPUT_ELEMENT_DESC layout[2] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	ThrowIfFalse(m_vs != nullptr);
	ThrowIfFalse(m_ps != nullptr);

	{
		D3D12_DESCRIPTOR_RANGE range = { };
		{
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.BaseShaderRegister = 0;
			range.NumDescriptors = 1;
		}

		D3D12_ROOT_PARAMETER rootParameter = { };
		{
			rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter.DescriptorTable.NumDescriptorRanges = 1;
			rootParameter.DescriptorTable.pDescriptorRanges = &range;
			rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

		D3D12_STATIC_SAMPLER_DESC sampler = {};
		{
			sampler.Filter = D3D12_FILTER_ANISOTROPIC;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 16;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			sampler.MinLOD = 0.f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		D3D12_ROOT_SIGNATURE_DESC rsDesc = { };
		{
			rsDesc.NumParameters = 1;
			rsDesc.pParameters = &rootParameter;
			rsDesc.NumStaticSamplers = 1;
			rsDesc.pStaticSamplers = &sampler;
			rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}

		ComPtr<ID3DBlob> rsBlob = nullptr;
		ComPtr<ID3DBlob> errBlob = nullptr;

		auto result = D3D12SerializeRootSignature(
			&rsDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			rsBlob.ReleaseAndGetAddressOf(),
			errBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			outputDebugMessage(errBlob.Get());
		}
		ThrowIfFailed(result);

		result = Resource::instance()->getDevice()->CreateRootSignature(
			0,
			rsBlob->GetBufferPointer(),
			rsBlob->GetBufferSize(),
			IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = { };
		{
			gpsDesc.pRootSignature = m_rootSignature.Get();
			gpsDesc.VS.pShaderBytecode = m_vs.Get()->GetBufferPointer();
			gpsDesc.VS.BytecodeLength = m_vs.Get()->GetBufferSize();
			gpsDesc.PS.pShaderBytecode = m_ps.Get()->GetBufferPointer();
			gpsDesc.PS.BytecodeLength = m_ps.Get()->GetBufferSize();
			//D3D12_STREAM_OUTPUT_DESC StreamOutput;
			gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			//D3D12_DEPTH_STENCIL_DESC DepthStencilState;
			gpsDesc.InputLayout.pInputElementDescs = layout;
			gpsDesc.InputLayout.NumElements = _countof(layout);
			//D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
			gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			gpsDesc.NumRenderTargets = 1;
			gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			//DXGI_FORMAT DSVFormat;
			gpsDesc.SampleDesc = { 1, 0 };
			//UINT NodeMask;
			//D3D12_CACHED_PIPELINE_STATE CachedPSO;
			gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpsDesc,
			IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Pera::render(ID3D12DescriptorHeap *pSrvDescHeap)
{
	ThrowIfFalse(pSrvDescHeap != nullptr);

	Resource::instance()->getCommandList()->SetGraphicsRootSignature(m_rootSignature.Get());
	Resource::instance()->getCommandList()->SetPipelineState(m_pipelineState.Get());

	Resource::instance()->getCommandList()->SetDescriptorHeaps(1, &pSrvDescHeap);
	D3D12_GPU_DESCRIPTOR_HANDLE handle = pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
	Resource::instance()->getCommandList()->SetGraphicsRootDescriptorTable(0, handle);

	{
		D3D12_VIEWPORT viewport = { };
		{
			viewport.Width = kWindowWidth;
			viewport.Height = kWindowHeight;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.MaxDepth = 1.0f;
			viewport.MinDepth = 0.0f;
		}
		Resource::instance()->getCommandList()->RSSetViewports(1, &viewport);
	}

	{
		D3D12_RECT scissorRect = { };
		{
			scissorRect.top = 0;
			scissorRect.left = 0;
			scissorRect.right = scissorRect.left + kWindowWidth;
			scissorRect.bottom = scissorRect.top + kWindowHeight;
		}
		Resource::instance()->getCommandList()->RSSetScissorRects(1, &scissorRect);
	}

	Resource::instance()->getCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Resource::instance()->getCommandList()->IASetVertexBuffers(0, 1, &m_peraVertexBufferView);
	Resource::instance()->getCommandList()->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Pera::createVertexBufferResource()
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
			IID_PPV_ARGS(m_peraVertexBuffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		PeraVertex* pMappedPera = nullptr;

		auto result = m_peraVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pMappedPera));
		ThrowIfFailed(result);

		std::copy(std::begin(pv), std::end(pv), pMappedPera);

		m_peraVertexBuffer->Unmap(0, nullptr);
	}

	{
		m_peraVertexBufferView.BufferLocation = m_peraVertexBuffer.Get()->GetGPUVirtualAddress();
		m_peraVertexBufferView.SizeInBytes = sizeof(pv);
		m_peraVertexBufferView.StrideInBytes = sizeof(PeraVertex);
	}

	return S_OK;
}

HRESULT Pera::createBokehResource()
{
	std::vector<float> weights = getGaussianWeights(8, 1.0f);
	ThrowIfFalse(weights.size() > 0);

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
		resourceDesc.Width = Util::alignmentedSize(sizeof(weights[0]) * weights.size(), 256);
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
		IID_PPV_ARGS(m_bokehParamBuffer.GetAddressOf()));
	ThrowIfFailed(result);

	float* pMappedWeight = nullptr;

	result = m_bokehParamBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pMappedWeight));
	ThrowIfFailed(result);
	{
		std::copy(weights.begin(), weights.end(), pMappedWeight);
	}
	m_bokehParamBuffer.Get()->Unmap(0, nullptr);

	return S_OK;
}

static std::vector<float> getGaussianWeights(size_t count, float sigma)
{
	std::vector<float> weights(count);
	float x = 0.0f;
	float total = 0.0f;

	for (auto& wgt : weights)
	{
		wgt = std::expf(-(x * x) / (2 * sigma * sigma));
		total += wgt;
		x += 1.0f;
	}

	total = total * 2.0f - 1.0f;

	for (auto& wgt : weights)
	{
		wgt /= total;
	}

	return weights;
}
