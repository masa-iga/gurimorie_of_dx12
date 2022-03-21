#include "ssao.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "config.h"
#include "constant.h"
#include "debug.h"
#include "init.h"
#include "toolkit.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Ssao::init(UINT64 width, UINT64 height)
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createPipelineState());
	ThrowIfFailed(createResource(width, static_cast<UINT>(height)));
	return S_OK;
}

void Ssao::setResource(TargetResource target, Microsoft::WRL::ComPtr<ID3D12Resource> resource)
{
	switch (target) {
	case TargetResource::kDstRt: m_dstResource = resource; break;
	case TargetResource::kSrcDepth: m_srcDepthResource = resource; break;
	case TargetResource::kSrcNormal: m_srcNormalResource = resource; break;
	default: Debug::debugOutputFormatString("illegal case. (%d)\n", target); ThrowIfFalse(false);
	}
}

HRESULT Ssao::render(ID3D12GraphicsCommandList* list)
{
	setupRenderTargetView();
	setupShaderResourceView();

	ThrowIfFailed(renderSsao(list));
	ThrowIfFailed(renderToTarget(list));

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

HRESULT Ssao::createResource(UINT64 dstWidth, UINT dstHeight)
{
	{
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			Constant::kDefaultRtFormat,
			dstWidth,
			dstHeight,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		const D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(resourceDesc.Format, kClearColor);

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearValue,
			IID_PPV_ARGS(m_workResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workResource.Get()->SetName(Util::getWideStringFromString("ssaoWorkResource").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 2,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_workDescHeapRtv.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workDescHeapRtv.Get()->SetName(Util::getWideStringFromString("ssaoWorkRtvDescHeap").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 2,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_workDescHeapSrv.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workDescHeapSrv.Get()->SetName(Util::getWideStringFromString("ssaoWorkSrvDescHeap").c_str());
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Ssao::createRootSignature()
{
	const D3D12_DESCRIPTOR_RANGE descRanges[] = {
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0 /* slot */),
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
		.InputLayout = { CommonResource::getInputElementDesc(), CommonResource::getInputElementNum() },
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

void Ssao::setupRenderTargetView()
{
	ThrowIfFalse(m_dstResource != nullptr);

	ID3D12Resource* resources[] = {
		m_workResource.Get(),
		m_dstResource.Get(),
	};

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = m_workDescHeapRtv.Get()->GetCPUDescriptorHandleForHeapStart();

	for (auto& resource : resources)
	{
		const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
			.Format = resource->GetDesc().Format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = { 0, 0 },
		};

		Resource::instance()->getDevice()->CreateRenderTargetView(
			resource,
			&rtvDesc,
			cpuDescHandle);

		cpuDescHandle.ptr += Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
}

void Ssao::setupShaderResourceView()
{
	ThrowIfFalse(m_srcDepthResource != nullptr);
	ThrowIfFalse(m_srcNormalResource != nullptr);

	ID3D12Resource* resources[] = {
		m_srcDepthResource.Get(),
		m_srcNormalResource.Get(),
	};

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = m_workDescHeapSrv.Get()->GetCPUDescriptorHandleForHeapStart();

	for (auto& resource : resources)
	{
		const DXGI_FORMAT format = (resource->GetDesc().Format == DXGI_FORMAT_R32_TYPELESS) ?
			DXGI_FORMAT_R32_FLOAT :
			resource->GetDesc().Format;

		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = format,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = 1,
				.PlaneSlice = 0,
				.ResourceMinLODClamp = 0.0f,
			},
		};

		Resource::instance()->getDevice()->CreateShaderResourceView(
			resource,
			&srvDesc,
			cpuDescHandle);

		cpuDescHandle.ptr += Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}
}

HRESULT Ssao::renderSsao(ID3D12GraphicsCommandList* list)
{
	list->SetGraphicsRootSignature(m_rootSignature.Get());
	list->SetPipelineState(m_pipelineState.Get());

	list->SetDescriptorHeaps(1, m_workDescHeapSrv.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(0 /* RootParameterIndex */, m_workDescHeapSrv.Get()->GetGPUDescriptorHandleForHeapStart());

	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Config::kWindowWidth), static_cast<float>(Config::kWindowHeight));
		list->RSSetViewports(1, &viewport);
	}

	{
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		list->RSSetScissorRects(1, &scissorRect);
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE dstRtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_workDescHeapRtv.Get()->GetCPUDescriptorHandleForHeapStart(),
		0,
		Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

	list->OMSetRenderTargets(1, &dstRtv, false, nullptr);
	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	list->IASetVertexBuffers(0, 1, CommonResource::getVertexBufferView());

	list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Ssao::renderToTarget(ID3D12GraphicsCommandList* list)
{
	// TODO: imple
	return S_OK;
}

