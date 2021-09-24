#include "floor.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <DirectXMath.h>
#include <d3dcompiler.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Floor::init()
{
	ThrowIfFailed(loadShaders());
	ThrowIfFailed(createVertexResource());
	ThrowIfFailed(createRootSignature());
	ThrowIfFailed(createGraphicsPipeline());
	return S_OK;
}

HRESULT Floor::renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);
	ThrowIfFalse(depthHeap != nullptr);

	setInputAssembler(list);
	setRasterizer(list);

	{
		auto depthHandle = depthHeap->GetCPUDescriptorHandleForHeapStart();
		list->OMSetRenderTargets(0, nullptr, false, &depthHandle);
	}

	list->SetPipelineState(m_shadowPipelineState.Get());
	list->SetGraphicsRootSignature(m_rootSignature.Get());

	list->SetDescriptorHeaps(1, &sceneDescHeap);
	list->SetGraphicsRootDescriptorTable(0, sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	list->DrawInstanced(6, 1, 0, 0);

	return S_OK;
}

HRESULT Floor::render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthLightSrvHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);

	setInputAssembler(list);
	setRasterizer(list);

	list->SetPipelineState(m_pipelineState.Get());
	list->SetGraphicsRootSignature(m_rootSignature.Get());

	list->SetDescriptorHeaps(1, &sceneDescHeap);
	list->SetGraphicsRootDescriptorTable(0 /* root param 0 */, sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	list->SetDescriptorHeaps(1, &depthLightSrvHeap);
	list->SetGraphicsRootDescriptorTable(1 /* root param 1 */, depthLightSrvHeap->GetGPUDescriptorHandleForHeapStart());

	list->DrawInstanced(6, 1, 0, 0);

	return S_OK;
}

HRESULT Floor::loadShaders()
{
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kVsBasicEntryPoint,
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vs.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kPsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kPsBasicEntryPoint,
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_ps.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kVsShadowEntryPoint,
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_shadowVs.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

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
		DirectX::XMFLOAT3(-kLength, kHeight, -kLength),
		DirectX::XMFLOAT3(-kLength, kHeight,  kLength),
		DirectX::XMFLOAT3( kLength, kHeight,  kLength),
		DirectX::XMFLOAT3(-kLength, kHeight, -kLength),
		DirectX::XMFLOAT3( kLength, kHeight,  kLength),
		DirectX::XMFLOAT3( kLength, kHeight, -kLength),
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

		result = m_vertResource.Get()->SetName(Util::getWideStringFromString("FloorVertexBuffer").c_str());
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

HRESULT Floor::createRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descRange[2] = { };
	{
		descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // b0: scene matrix
		descRange[0].NumDescriptors = 1;
		descRange[0].BaseShaderRegister = 0;
		descRange[0].RegisterSpace = 0;
	    descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t0: depth light map
		descRange[1].NumDescriptors = 1;
		descRange[1].BaseShaderRegister = 0;
		descRange[1].RegisterSpace = 0;
	    descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_ROOT_PARAMETER rootParam[2] = { };
	{
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[0].DescriptorTable.pDescriptorRanges = &descRange[0];
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[1].DescriptorTable.pDescriptorRanges = &descRange[1];
		rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);
	{
		//D3D12_FILTER Filter;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		//FLOAT MipLODBias;
		//UINT MaxAnisotropy;
		//D3D12_COMPARISON_FUNC ComparisonFunc;
		//D3D12_STATIC_BORDER_COLOR BorderColor;
		//FLOAT MinLOD;
		//FLOAT MaxLOD;
		//UINT ShaderRegister;
		//UINT RegisterSpace;
		//D3D12_SHADER_VISIBILITY ShaderVisibility;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	{
		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = &rootParam[0];
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &samplerDesc;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.GetAddressOf(),
		errorBlob.GetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = Resource::instance()->getDevice()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("FloorRootSignature").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Floor::createGraphicsPipeline()
{
	constexpr D3D12_INPUT_ELEMENT_DESC kInputElementDesc[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = { };
	{
		ThrowIfFalse(m_rootSignature != nullptr);
		pipelineStateDesc.pRootSignature = m_rootSignature.Get();
		pipelineStateDesc.VS = { m_vs.Get()->GetBufferPointer(), m_vs.Get()->GetBufferSize() };
		pipelineStateDesc.PS = { m_ps.Get()->GetBufferPointer(), m_ps.Get()->GetBufferSize() };
		//D3D12_SHADER_BYTECODE DS;
		//D3D12_SHADER_BYTECODE HS;
		//D3D12_SHADER_BYTECODE GS;
		//D3D12_STREAM_OUTPUT_DESC StreamOutput;
		pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		{
			pipelineStateDesc.RasterizerState.FrontCounterClockwise = true;
			pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			pipelineStateDesc.RasterizerState.DepthClipEnable = false;
		}
		pipelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pipelineStateDesc.InputLayout = { kInputElementDesc, _countof(kInputElementDesc) };
		//D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
		pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc.NumRenderTargets = 1;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineStateDesc.SampleDesc = { 1, 0 };
		//UINT NodeMask;
		//D3D12_CACHED_PIPELINE_STATE CachedPSO;
		//D3D12_PIPELINE_STATE_FLAGS Flags;
	}

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("FloorGraphicsPipeline").c_str());
		ThrowIfFailed(result);
	}

	// for shadow
	{
		pipelineStateDesc.VS = { m_shadowVs.Get()->GetBufferPointer(), m_shadowVs.Get()->GetBufferSize() };
		pipelineStateDesc.PS = { nullptr, 0 };
		pipelineStateDesc.NumRenderTargets = 0;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	}

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(m_shadowPipelineState.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_shadowPipelineState.Get()->SetName(Util::getWideStringFromString("FloorGraphicsShadowPipeline").c_str());
		ThrowIfFailed(result);
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
			viewport.MaxDepth = 1.0f;
		}
		list->RSSetViewports(1, &viewport);
	}

	{
		D3D12_RECT scissorRect = { };
		{
			scissorRect.left = 0;
			scissorRect.top = 0;
			scissorRect.right = scissorRect.left + kWindowWidth;
			scissorRect.bottom = scissorRect.top + kWindowHeight;
		}
		list->RSSetScissorRects(1, &scissorRect);
	}
}
