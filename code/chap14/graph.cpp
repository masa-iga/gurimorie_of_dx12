#include "graph.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <algorithm>
#include <d3dcompiler.h>
#pragma warning(pop)
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

RenderGraph::RenderGraph()
{
	m_dataArray.fill(0.0f);
}

HRESULT RenderGraph::init()
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createVertexBuffer());
	ThrowIfFailed(createPipelineState());
	return S_OK;
}

void RenderGraph::set(float val)
{
	m_dataArray.at(m_wrIdx) = val;
	m_wrIdx = (m_wrIdx + 1 == kNumElements) ? 0 : m_wrIdx + 1;
}

void RenderGraph::update()
{
	const std::array<Vertex, kNumMaxVertices> vertices = createVertices();
	uploadVertices(vertices);
}

HRESULT RenderGraph::render(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
	ThrowIfFalse(list != nullptr);

	list->SetGraphicsRootSignature(m_rootSignature.Get());
	list->SetPipelineState(m_pipelineState.Get());

	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissorRect);

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
	list->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	list->DrawInstanced(kNumMaxVertices, 1, 0, 0);

	return S_OK;
}

HRESULT RenderGraph::compileShaders()
{
	ComPtr<ID3DBlob> errBlob = nullptr;

	auto result = D3DCompileFromFile(
		L"graphVertex.hlsl",
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
		L"graphPixel.hlsl",
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

HRESULT RenderGraph::createVertexBuffer()
{
	constexpr size_t vertexBufferSize = sizeof(Vertex) * kNumMaxVertices;

	const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

	auto result = Resource::instance()->getDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_vertexBuffer.Get()->SetName(Util::getWideStringFromString("vertexBufferGraph").c_str());
	ThrowIfFailed(result);

	m_vertexBufferView = {
		.BufferLocation = m_vertexBuffer.Get()->GetGPUVirtualAddress(),
		.SizeInBytes = vertexBufferSize,
		.StrideInBytes = sizeof(Vertex),
	};

	return S_OK;
}

HRESULT RenderGraph::createPipelineState()
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

	result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("graphRootSignature").c_str());
	ThrowIfFailed(result);


	constexpr D3D12_INPUT_ELEMENT_DESC inputElementDesc[1] = {
		{
			.SemanticName = "POSITION",
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
		.VS = { m_vs.Get()->GetBufferPointer(), m_vs.Get()->GetBufferSize() },
		.PS = { m_ps.Get()->GetBufferPointer(), m_ps.Get()->GetBufferSize() },
		.DS = { nullptr, 0 },
		.HS = { nullptr, 0 },
		.GS = { nullptr, 0 },
		.StreamOutput = { nullptr, 0, nullptr, 0 , 0 },
		.BlendState = CD3DX12_BLEND_DESC(CD3DX12_DEFAULT()),
		.SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
		.RasterizerState = CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT()),
		.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT()),
		.InputLayout = { inputElementDesc, _countof(inputElementDesc) },
		.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
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

	result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("pipelineStateGraph").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

std::array<RenderGraph::Vertex, RenderGraph::kNumMaxVertices> RenderGraph::createVertices() const
{
	std::array<Vertex, kNumMaxVertices> array = { };

	constexpr float unit = (2.0f / kNumElements);
	constexpr float z = 0.1f;

	for (uint32_t i = 0; i < array.size(); ++i)
	{
		array.at(i) = Vertex(DirectX::XMFLOAT3(-1.0f + i * unit, 0.0f, z));
	}

	constexpr float min = 0.0f;
	constexpr float max = 17.0f;

	for (size_t i = 0; i < kNumElements; ++i)
	{
		const size_t idx = i < m_wrIdx ?
			(m_wrIdx - 1) - i :
			(kNumElements - 1) - (i - m_wrIdx);
		const float v = std::clamp(m_dataArray.at(idx), min, max) / std::abs(max);

		array.at(i).pos.y = v;
	}

	return array;
}

void RenderGraph::uploadVertices(const std::array<Vertex, kNumMaxVertices> vertices)
{
	Vertex* pMappedVertex = nullptr;

	// map
	{
		auto result = m_vertexBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pMappedVertex));
		ThrowIfFailed(result);
	}

	{
		std::copy(vertices.begin(), vertices.end(), pMappedVertex);
	}

	// unmap
	{
		m_vertexBuffer.Get()->Unmap(0, nullptr);
	}
}

