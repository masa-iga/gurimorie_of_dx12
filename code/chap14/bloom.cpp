#include "bloom.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "config.h"
#include "constant.h"
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Bloom::init(UINT64 width, UINT height)
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createResource(width, height));
	ThrowIfFailed(createPipelineState());
	return S_OK;
}

HRESULT Bloom::render(ID3D12GraphicsCommandList* list, const D3D12_CPU_DESCRIPTOR_HANDLE *pDstRtv, ID3D12DescriptorHeap *pSrcTexDescHeap)
{
	list->SetGraphicsRootSignature(m_rootSignature.Get());
	list->SetPipelineState(m_pipelineState.Get());

//	list->SetDescriptorHeaps();
//	list->SetGraphicsRootDescriptorTable(0, 0);

	list->OMSetRenderTargets(1, pDstRtv, false, nullptr);

	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Config::kWindowWidth), static_cast<float>(Config::kWindowHeight));
		list->RSSetViewports(1, &viewport);
	}

	{
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		list->RSSetScissorRects(1, &scissorRect);
	}

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
//	list->IASetVertexBuffers(0, 1, nullptr);
//	list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Bloom::compileShaders()
{
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsBlob.ReleaseAndGetAddressOf(),
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
		"main",
		Constant::kPsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_psBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Bloom::createResource(UINT64 width, UINT height)
{
    const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);

	for (auto& resource : m_buffers)
	{
		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = resource.Get()->SetName(Util::getWideStringFromString("bloomBuffer").c_str());
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Bloom::createRootSignature()
{
	const D3D12_DESCRIPTOR_RANGE descRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	const D3D12_ROOT_DESCRIPTOR_TABLE descTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRange);

	const D3D12_ROOT_PARAMETER rootParam = {
		.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
		.DescriptorTable = descTable,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
	};

	const D3D12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		1,
		&rootParam,
		1,
		&samplerDesc,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);


	ret = Resource::instance()->getDevice()->CreateRootSignature(
		0 /* nodeMask */,
		rootSigBlob.Get()->GetBufferPointer(),
		rootSigBlob.Get()->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	ret = m_rootSignature.Get()->SetName(Util::getWideStringFromString("bloomRootSignature").c_str());
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Bloom::createPipelineState()
{
	ThrowIfFailed(createRootSignature());

	constexpr D3D12_INPUT_ELEMENT_DESC inputElementDesc[2] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		},
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpDesc = {
		.pRootSignature = m_rootSignature.Get(),
		.VS = { m_vsBlob.Get()->GetBufferPointer(), m_vsBlob.Get()->GetBufferSize() },
		.PS = { m_psBlob.Get()->GetBufferPointer(), m_psBlob.Get()->GetBufferSize() },
		.DS = { nullptr, 0 },
		.HS = { nullptr, 0 },
		.GS = { nullptr, 0 },
		.StreamOutput = { nullptr, 0, nullptr, 0, 0 },
		.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT()),
		.SampleMask = 0,
		.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT()),
		.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT()),
		.InputLayout = { inputElementDesc, _countof(inputElementDesc) },
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = 1,
		.RTVFormats = { },
		.DSVFormat = DXGI_FORMAT_UNKNOWN,
		.SampleDesc = { 1, 0 },
		.NodeMask = 0,
		.CachedPSO = { nullptr, 0 },
		.Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
	};
	gpDesc.DepthStencilState.DepthEnable = false;
	gpDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpDesc,
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("bloomPipelineState").c_str());
	ThrowIfFailed(result);

	return S_OK;
}
