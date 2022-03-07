#include "ssao.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "constant.h"
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Ssao::init(UINT64 width, UINT64 height)
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createPipelineState());
	return S_OK;
}

HRESULT Ssao::render(ID3D12GraphicsCommandList* list)
{
	// set root signature
	// set pipeline state
	// set descriptor heap
	// set graphics root descriptor table
	// viewport
	// scissor
	// set render target
	// set primitive type
	// set vertex buffer
	// draw
	return S_OK;
}

HRESULT Ssao::compileShaders()
{
	ComPtr<ID3DBlob> errorBlob = nullptr;

	{
		auto result = D3DCompileFromFile(
			kVsFile,
			Constant::kCompileShaderDefines,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			kVsEntryPoint,
			Constant::kVsShaderModel,
			Constant::kCompileShaderFlags1,
			Constant::kCompileShaderFlags2,
			m_vsBlob.ReleaseAndGetAddressOf(),
			errorBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			Debug::outputDebugMessage(errorBlob.Get());
			return E_FAIL;
		}
	}

	{
		auto result = D3DCompileFromFile(
			kPsFile,
			Constant::kCompileShaderDefines,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			kPsEntryPoint,
			Constant::kPsShaderModel,
			Constant::kCompileShaderFlags1,
			Constant::kCompileShaderFlags2,
			m_psBlob.ReleaseAndGetAddressOf(),
			errorBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			Debug::outputDebugMessage(errorBlob.Get());
			return E_FAIL;
		}
	}

	return S_OK;
}

HRESULT Ssao::createRootSignature()
{
	const D3D12_DESCRIPTOR_RANGE descRanges[] = {
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 /* slot */),
	};

	const D3D12_ROOT_PARAMETER rootParams[] = {
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRanges[0]),
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
		},
	};

	const D3D12_STATIC_SAMPLER_DESC samplerDescs[] = {
		CD3DX12_STATIC_SAMPLER_DESC(0 /* slot */, D3D12_FILTER_MIN_MAG_MIP_LINEAR),
	};

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		_countof(rootParams),
		rootParams,
		_countof(samplerDescs),
		samplerDescs,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		Constant::kRootSignatureVersion,
		rootSigBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(ret))
	{
		Debug::outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);

	auto result = Resource::instance()->getDevice()->CreateRootSignature(
		0 /* nodeMask */,
		rootSigBlob.Get()->GetBufferPointer(),
		rootSigBlob.Get()->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("ssaoRootSignature").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Ssao::createPipelineState()
{
	ThrowIfFailed(createRootSignature());

	D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		},
		{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
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
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
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
	gpDesc.RTVFormats[0] = Constant::kDefaultRtFormat;

	auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpDesc,
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("ssaoPipelineState").c_str());
	ThrowIfFailed(result);

	return S_OK;
}
