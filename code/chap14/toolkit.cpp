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
	ThrowIfFailed(createConstantBuffer());
	ThrowIfFailed(createPipelineState());
	ThrowIfFailed(uploadVertices());
	ThrowIfFailed(uploadOutputColor(kDefaultOutputColor));
	return S_OK;
}

void Toolkit::teardown()
{
	m_vs.Reset();
	m_ps.Reset();
	m_vertexBuffer.Reset();
	m_rootSignature.Reset();
	m_pipelineState.Reset();
	m_pipelineStateBlend.Reset();
}

HRESULT Toolkit::drawClear(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
	return drawClearInternal(list, viewport, scissorRect, false);
}

HRESULT Toolkit::drawClearBlend(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
	return drawClearInternal(list, viewport, scissorRect, true);
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
	const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(kVertexBufferSize);

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
		.SizeInBytes = kVertexBufferSize,
		.StrideInBytes = sizeof(Vertex),
	};

	return S_OK;
}

HRESULT Toolkit::createConstantBuffer()
{
	const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Util::alignmentedSize(sizeof(OutputColor), 256));

	{
		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_constantOutputColorBuffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_constantOutputColorBuffer.Get()->SetName(Util::getWideStringFromString("constantOutputColorBufferToolkit").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_DESCRIPTOR_HEAP_DESC descHeap = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
            &descHeap,
			IID_PPV_ARGS(m_constantOutputColorHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		const D3D12_CONSTANT_BUFFER_VIEW_DESC bufferView = {
			.BufferLocation = m_constantOutputColorBuffer.Get()->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(m_constantOutputColorBuffer.Get()->GetDesc().Width),
		};
		D3D12_CPU_DESCRIPTOR_HANDLE descHandle = m_constantOutputColorHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		Resource::instance()->getDevice()->CreateConstantBufferView(&bufferView, descHandle);
	}

	return S_OK;
}

HRESULT Toolkit::createPipelineState()
{
	const D3D12_DESCRIPTOR_RANGE descRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	const D3D12_ROOT_DESCRIPTOR_TABLE descTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRange);
	const D3D12_ROOT_PARAMETER rootParam = {
		.DescriptorTable = descTable,
		.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
	};

	const D3D12_ROOT_SIGNATURE_DESC rsDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		1,
		&rootParam,
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

	{
		D3D12_RENDER_TARGET_BLEND_DESC blendDesc = {
		.BlendEnable = true,
		.LogicOpEnable = false,
		.SrcBlend = D3D12_BLEND_SRC_ALPHA,
		.DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
		.BlendOp = D3D12_BLEND_OP_ADD,
		.SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA,
		.DestBlendAlpha = D3D12_BLEND_SRC_ALPHA,
		.BlendOpAlpha = D3D12_BLEND_OP_MAX,
		.LogicOp = D3D12_LOGIC_OP_CLEAR,
		.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		gpDesc.BlendState.RenderTarget[0] = blendDesc;
	}

	result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpDesc,
		IID_PPV_ARGS(m_pipelineStateBlend.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_pipelineStateBlend.Get()->SetName(Util::getWideStringFromString("pipelineStateBlendToolkit").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Toolkit::uploadVertices()
{
	constexpr Vertex vertices[kNumVertices] = {
		DirectX::XMFLOAT3(-1.0f, -1.0f, 0.1f),
		DirectX::XMFLOAT3(-1.0f,  3.0f, 0.1f),
		DirectX::XMFLOAT3( 3.0f, -1.0f, 0.1f),
	};

	Vertex* pVertices = nullptr;
	auto result = m_vertexBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
	ThrowIfFailed(result);

	std::copy(std::begin(vertices), std::end(vertices), pVertices);

	m_vertexBuffer.Get()->Unmap(0, nullptr);

	return S_OK;
}

HRESULT Toolkit::uploadOutputColor(OutputColor outputColor)
{
	OutputColor* pOutputColor = nullptr;
	auto result = m_constantOutputColorBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pOutputColor));
	ThrowIfFailed(result);

	*pOutputColor = outputColor;

	m_constantOutputColorBuffer.Get()->Unmap(0, nullptr);

	return S_OK;
}

HRESULT Toolkit::drawClearInternal(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect, bool blend)
{
	ThrowIfFalse(list != nullptr);

	list->SetGraphicsRootSignature(m_rootSignature.Get());

	if (blend)
	{
		list->SetPipelineState(m_pipelineStateBlend.Get());
	}
	else
	{
		list->SetPipelineState(m_pipelineState.Get());
	}

	list->SetDescriptorHeaps(1, m_constantOutputColorHeap.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(0, m_constantOutputColorHeap.Get()->GetGPUDescriptorHandleForHeapStart());

	list->RSSetViewports(1, &viewport);
	list->RSSetScissorRects(1, &scissorRect);

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	list->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	list->DrawInstanced(3, 1, 0, 0);

	return S_OK;
}

