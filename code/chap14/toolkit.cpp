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

namespace {
	const std::vector<Toolkit::Vertex> kVertices0 = {
		{ DirectX::XMFLOAT3(-1.0f, -1.0f, 0.1f) },
		{ DirectX::XMFLOAT3(-1.0f,  3.0f, 0.1f) },
		{ DirectX::XMFLOAT3( 3.0f, -1.0f, 0.1f) },
	};

	const std::vector<Toolkit::Vertex> kVertices1 = {
		{ DirectX::XMFLOAT3(-1.0f, -1.0f, 0.1f) },
		{ DirectX::XMFLOAT3(-1.0f,  1.0f, 0.1f) },
		{ DirectX::XMFLOAT3( 1.0f,  1.0f, 0.1f) },
		{ DirectX::XMFLOAT3( 1.0f, -1.0f, 0.1f) },
	};
} // anonymous namespace

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
	for (auto& vs : m_vsArray)
		vs.Reset();

	for (auto& ps : m_psArray)
		ps.Reset();

	for (auto& buffer : m_vertexBuffers)
		buffer.Reset();

	m_constantOutputColorBuffer.Reset();
	m_constantOutputColorHeap.Reset();
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

HRESULT Toolkit::drawRect(ID3D12GraphicsCommandList* list, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
	return S_OK;
}

HRESULT Toolkit::compileShaders()
{
	ComPtr<ID3DBlob> errBlob = nullptr;

	{
		auto result = D3DCompileFromFile(
			L"toolkit_vs.hlsl",
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main",
			"vs_5_0",
			0,
			0,
			m_vsArray.at(static_cast<size_t>(DrawType::kClear)).ReleaseAndGetAddressOf(),
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
			m_psArray.at(static_cast<size_t>(DrawType::kClear)).ReleaseAndGetAddressOf(),
			errBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			outputDebugMessage(errBlob.Get());
			ThrowIfFalse(false);
		}
	}

	{
		m_vsArray.at(static_cast<size_t>(DrawType::kRect)) = m_vsArray.at(static_cast<size_t>(DrawType::kClear)).Get();
		m_psArray.at(static_cast<size_t>(DrawType::kRect)) = m_psArray.at(static_cast<size_t>(DrawType::kClear)).Get();
	}

	return S_OK;
}

HRESULT Toolkit::createVertexBuffer()
{
	for (size_t i = 0; i < static_cast<size_t>(DrawType::kEnd); ++i)
	{
		const size_t bufferSize = kVertexBufferSizes.at(i);
		auto& buffer = m_vertexBuffers.at(i);

		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(buffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		const std::string s = "vertexBufferToolkit" + std::to_string(i);
		result = buffer.Get()->SetName(Util::getWideStringFromString(s).c_str());
		ThrowIfFailed(result);

		m_vertexBufferViews.at(i) = {
			.BufferLocation = buffer.Get()->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<UINT>(bufferSize),
			.StrideInBytes = sizeof(Vertex),
		};
	}

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

		result = m_constantOutputColorHeap.Get()->SetName(Util::getWideStringFromString("constantOutputColorHeapToolkit").c_str());
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
		.VS = { m_vsArray.at((size_t)(DrawType::kClear)).Get()->GetBufferPointer(), m_vsArray.at((size_t)(DrawType::kClear)).Get()->GetBufferSize() },
		.PS = { m_psArray.at((size_t)(DrawType::kClear)).Get()->GetBufferPointer(), m_psArray.at((size_t)(DrawType::kClear)).Get()->GetBufferSize() },
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
	const std::array<std::vector<Vertex>, static_cast<size_t>(DrawType::kEnd)> vertexArray = { kVertices0, kVertices1 };

	for (size_t i = 0; i < static_cast<size_t>(DrawType::kEnd); ++i)
	{
		Vertex* pDst = nullptr;
		auto result = m_vertexBuffers.at(i).Get()->Map(0, nullptr, reinterpret_cast<void**>(&pDst));
		ThrowIfFailed(result);

		std::copy(std::begin(vertexArray.at(i)), std::end(vertexArray.at(i)), pDst);

		m_vertexBuffers.at(i).Get()->Unmap(0, nullptr);
	}

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
	list->IASetVertexBuffers(0, 1, &m_vertexBufferViews.at(static_cast<size_t>(DrawType::kClear)));

	list->DrawInstanced(3, 1, 0, 0);

	return S_OK;
}

