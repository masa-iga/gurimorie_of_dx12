#include "toolkit.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Toolkit::init()
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createVertexBuffer());
	ThrowIfFailed(createPipelineState());
	ThrowIfFailed(uploadVertices());
	return S_OK;
}

void Toolkit::teardown()
{
	m_vs.Reset();
	m_ps.Reset();
	m_vertexBuffer.Reset();
	m_rootSignature.Reset();
	m_pipelineState.Reset();
}

HRESULT Toolkit::drawClear(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
	ThrowIfFalse(list != nullptr);

	list->SetGraphicsRootSignature(m_rootSignature.Get());
	list->SetPipelineState(m_pipelineState.Get());

	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissorRect);

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	list->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Toolkit::compileShaders()
{
	ComPtr<ID3DBlob> errBlob = nullptr;

	auto result = D3DCompileFromFile(
		L"toolkit_vs.hlsl",
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
		ThrowIfFalse(false);
	}

	result = D3DCompileFromFile(
		L"toolkit_ps.hlsl",
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
		ThrowIfFalse(false);
	}

	return S_OK;
}

HRESULT Toolkit::createVertexBuffer()
{
	constexpr size_t vertexBufferSize = sizeof(Vertex) * kNumVertices;

	const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

	auto result = Resource::instance()->getDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_vertexBuffer.Get()->SetName(Util::getWideStringFromString("vertexBufferToolkit").c_str());
	ThrowIfFailed(result);

	m_vertexBufferView = {
		.BufferLocation = m_vertexBuffer.Get()->GetGPUVirtualAddress(),
		.SizeInBytes = vertexBufferSize,
		.StrideInBytes = sizeof(Vertex),
	};

	return S_OK;
}

HRESULT Toolkit::createPipelineState()
{
	const D3D12_ROOT_SIGNATURE_DESC rsDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		0,
		nullptr,
		0,
		nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

	result = Resource::instance()->getDevice()->CreateRootSignature(
		0,
		rsBlob.Get()->GetBufferPointer(),
		rsBlob.Get()->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("RootSignatureToolkit").c_str());
	ThrowIfFailed(result);

	constexpr D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {
		{
			.SemanticName = "POSITION",
			.SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32_FLOAT,
			.InputSlot = 0,
			.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpDesc = {
		.pRootSignature = m_rootSignature.Get(),
		.VS = { m_vs.Get()->GetBufferPointer(), m_vs.Get()->GetBufferSize() },
		.PS = { m_ps.Get()->GetBufferPointer(), m_ps.Get()->GetBufferSize() },
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
	gpDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpDesc,
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("pipelineStateToolkit").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Toolkit::uploadVertices()
{
	// TODO: imple
	m_vertexBuffer.Get()->GetDesc().Width;

	Vertex* pVertices = nullptr;

	auto result = m_vertexBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
	ThrowIfFailed(result);

	m_vertexBuffer.Get()->Unmap(0, nullptr);

	return S_OK;
}

