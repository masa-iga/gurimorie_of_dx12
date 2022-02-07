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

constexpr FLOAT kClearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};

HRESULT Bloom::init(UINT64 width, UINT height)
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createResource(width, height));
	ThrowIfFailed(createPipelineState());
	return S_OK;
}

HRESULT Bloom::clearWorkRenderTarget(ID3D12GraphicsCommandList* list)
{
	list->ClearRenderTargetView(
		m_workDescHeapRtv.Get()->GetCPUDescriptorHandleForHeapStart(),
		kClearColor,
		0,
		nullptr
	);

	return S_OK;
}

HRESULT Bloom::render(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap* pSrcTexDescHeap, D3D12_GPU_DESCRIPTOR_HANDLE srcTexHandle, D3D12_GPU_DESCRIPTOR_HANDLE srcLumHandle)
{
	list->SetGraphicsRootSignature(m_rootSignatures.at(static_cast<size_t>(Type::kMain)).Get());
	list->SetPipelineState(m_pipelineStates.at(static_cast<size_t>(Type::kMain)).Get());

	list->SetDescriptorHeaps(1, &pSrcTexDescHeap);
	list->SetGraphicsRootDescriptorTable(static_cast<UINT>(Slot::kSrcTex), srcTexHandle);
	list->SetGraphicsRootDescriptorTable(static_cast<UINT>(Slot::kSrcLuminance), srcLumHandle);

	list->SetDescriptorHeaps(1, m_workDescHeapSrv.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(static_cast<UINT>(Slot::kShrinkLuminance), m_workDescHeapSrv.Get()->GetGPUDescriptorHandleForHeapStart());

	list->OMSetRenderTargets(1, &dstRtv, false, nullptr);

	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Config::kWindowWidth), static_cast<float>(Config::kWindowHeight));
		list->RSSetViewports(1, &viewport);
	}

	{
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		list->RSSetScissorRects(1, &scissorRect);
	}

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	list->IASetVertexBuffers(0, 1, &m_vbView);
	list->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Bloom::renderShrinkTextureForBlur(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* pSrcTexDescHeap, D3D12_GPU_DESCRIPTOR_HANDLE srcLumHandle)
{
	// TODO: barrier‚ð‚±‚±‚É“ü‚ê‚é‚×‚«‚©H
	//   input: high luminance buffer
	//   output: shrink buffer

	const int32_t baseWidth = Config::kWindowWidth / 2;
	const int32_t baseHeight = Config::kWindowHeight / 2;

	list->SetGraphicsRootSignature(m_rootSignatures.at(static_cast<size_t>(Type::kTexCopy)).Get());
	list->SetPipelineState(m_pipelineStates.at(static_cast<size_t>(Type::kTexCopy)).Get());

	list->SetDescriptorHeaps(1, &pSrcTexDescHeap);
	list->SetGraphicsRootDescriptorTable(static_cast<UINT>(Slot::kSrcLuminance), srcLumHandle);

	list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	list->IASetVertexBuffers(0, 1, &m_vbView);

	{
		const D3D12_CPU_DESCRIPTOR_HANDLE rtDescHandle[] = { m_workDescHeapRtv.Get()->GetCPUDescriptorHandleForHeapStart() };
		list->OMSetRenderTargets(1, rtDescHandle, false, nullptr);
	}

	D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(baseWidth), static_cast<float>(baseHeight));
	D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, baseWidth, baseHeight);

	for (uint32_t i = 0; i < 8; ++i)
	{
		list->RSSetViewports(1, &viewport);
		list->RSSetScissorRects(1, &scissorRect);
		list->DrawInstanced(4, 1, 0, 0);

		scissorRect.top += static_cast<LONG>(viewport.Height);
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = static_cast<float>(scissorRect.top);
		viewport.Width /= 2.0f;
		viewport.Height /= 2.0f;
		scissorRect.bottom = scissorRect.top + static_cast<LONG>(viewport.Height);
	}

	return S_OK;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Bloom::getWorkResource()
{
	return m_workResource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Bloom::getSrvWorkDescriptorHeap()
{
	return m_workDescHeapSrv;
}

HRESULT Bloom::compileShaders()
{
	for (uint32_t i = 0; i < m_vsBlobs.size(); ++i)
	{
		if (static_cast<size_t>(Type::kTexCopy) == i)
		{
			m_vsBlobs.at(static_cast<size_t>(Type::kTexCopy)) = m_vsBlobs.at(static_cast<size_t>(Type::kMain));
			continue;
		}

		ComPtr<ID3DBlob> errorBlob = nullptr;

		auto result = D3DCompileFromFile(
			kVsFile,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			kVsEntryPoints.at(i),
			Constant::kVsShaderModel,
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			m_vsBlobs.at(i).ReleaseAndGetAddressOf(),
			errorBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			Debug::outputDebugMessage(errorBlob.Get());
			return E_FAIL;
		}
	}

	for (uint32_t i = 0; i < m_psBlobs.size(); ++i)
	{
		ComPtr<ID3DBlob> errorBlob = nullptr;

		auto result = D3DCompileFromFile(
			kPsFile,
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			kPsEntryPoints.at(i),
			Constant::kPsShaderModel,
			D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
			0,
			m_psBlobs.at(i).ReleaseAndGetAddressOf(),
			errorBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			Debug::outputDebugMessage(errorBlob.Get());
			return E_FAIL;
		}
	}

	return S_OK;
}

HRESULT Bloom::createResource(UINT64 dstWidth, UINT dstHeight)
{
	struct VertexBuffer
	{
		DirectX::XMFLOAT3 pos = { };
		DirectX::XMFLOAT2 uv = { };
	};

	constexpr VertexBuffer vb[] = {
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
		{{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
	};

	{
		const UINT64 width = dstWidth / 2;
		constexpr float clearValue[] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			Constant::kDefaultRtFormat,
			width,
			dstHeight,
			1,
			0,
			1,
			0,
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
		const D3D12_CLEAR_VALUE clearColor = CD3DX12_CLEAR_VALUE(resourceDesc.Format, clearValue);

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			&clearColor,
			IID_PPV_ARGS(m_workResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workResource.Get()->SetName(Util::getWideStringFromString("bloomWorkBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_workDescHeapRtv.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workDescHeapRtv.Get()->SetName(Util::getWideStringFromString("bloomWorkRtvHeap").c_str());
		ThrowIfFailed(result);

		const D3D12_RENDER_TARGET_VIEW_DESC rtViewDesc = {
			.Format = m_workResource.Get()->GetDesc().Format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = { 0, 0 },
		};

		Resource::instance()->getDevice()->CreateRenderTargetView(
			m_workResource.Get(),
            &rtViewDesc,
            m_workDescHeapRtv.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	{
		const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
			.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
			.NumDescriptors = 1,
			.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
			.NodeMask = 0,
		};

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_workDescHeapSrv.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_workDescHeapSrv.Get()->SetName(Util::getWideStringFromString("bloomWorkSrvHeap").c_str());
		ThrowIfFailed(result);

		const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
			.Format = m_workResource.Get()->GetDesc().Format,
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
			m_workResource.Get(),
            &srvDesc,
            m_workDescHeapSrv.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	{
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vb));

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
			IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_vertexBuffer.Get()->SetName(Util::getWideStringFromString("bloomVertexBuffer").c_str());
		ThrowIfFailed(result);

		m_vbView = {
			.BufferLocation = m_vertexBuffer.Get()->GetGPUVirtualAddress(),
			.SizeInBytes = sizeof(vb),
			.StrideInBytes = sizeof(VertexBuffer),
		};
	}

	{
		VertexBuffer* data = nullptr;
		auto result = m_vertexBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&data));
		ThrowIfFailed(result);

		std::copy(std::begin(vb), std::end(vb), data);

		m_vertexBuffer.Get()->Unmap(0, nullptr);
	}

	return S_OK;
}

HRESULT Bloom::createRootSignature()
{
	const D3D12_DESCRIPTOR_RANGE descRanges[] = {
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, static_cast<UINT>(Slot::kSrcTex)),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, static_cast<UINT>(Slot::kSrcLuminance)),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, static_cast<UINT>(Slot::kShrinkLuminance)),
	};

	const D3D12_ROOT_PARAMETER rootParams[] = {
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRanges[0]),
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRanges[1]),
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(1, &descRanges[2]),
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
	};

	const D3D12_STATIC_SAMPLER_DESC samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		_countof(rootParams),
		rootParams,
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
		Debug::outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);


	{
		ret = Resource::instance()->getDevice()->CreateRootSignature(
			0 /* nodeMask */,
			rootSigBlob.Get()->GetBufferPointer(),
			rootSigBlob.Get()->GetBufferSize(),
			IID_PPV_ARGS(m_rootSignatures.at(static_cast<size_t>(Type::kMain)).ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);

		ret = m_rootSignatures.at(static_cast<size_t>(Type::kMain)).Get()->SetName(Util::getWideStringFromString("bloomMainRootSignature").c_str());
		ThrowIfFailed(ret);
	}

	{
		m_rootSignatures.at(static_cast<size_t>(Type::kTexCopy)) = m_rootSignatures.at(static_cast<size_t>(Type::kMain));

		ret = m_rootSignatures.at(static_cast<size_t>(Type::kTexCopy)).Get()->SetName(Util::getWideStringFromString("bloomTexCopyRootSignature").c_str());
	}

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
		.pRootSignature = m_rootSignatures.at(static_cast<size_t>(Type::kMain)).Get(),
		.VS = {
			m_vsBlobs.at(static_cast<size_t>(Type::kMain)).Get()->GetBufferPointer(),
			m_vsBlobs.at(static_cast<size_t>(Type::kMain)).Get()->GetBufferSize() },
		.PS = {
			m_psBlobs.at(static_cast<size_t>(Type::kMain)).Get()->GetBufferPointer(),
			m_psBlobs.at(static_cast<size_t>(Type::kMain)).Get()->GetBufferSize() },
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

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpDesc,
			IID_PPV_ARGS(m_pipelineStates.at(static_cast<size_t>(Type::kMain)).ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineStates.at(static_cast<size_t>(Type::kMain)).Get()->SetName(Util::getWideStringFromString("bloomMainPipelineState").c_str());
		ThrowIfFailed(result);
	}

	{
		gpDesc.pRootSignature = m_rootSignatures.at(static_cast<size_t>(Type::kTexCopy)).Get();
		gpDesc.VS = {
			m_vsBlobs.at(static_cast<size_t>(Type::kTexCopy)).Get()->GetBufferPointer(),
			m_vsBlobs.at(static_cast<size_t>(Type::kTexCopy)).Get()->GetBufferSize() };
		gpDesc.PS = {
			m_psBlobs.at(static_cast<size_t>(Type::kTexCopy)).Get()->GetBufferPointer(),
			m_psBlobs.at(static_cast<size_t>(Type::kTexCopy)).Get()->GetBufferSize() };

		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpDesc,
			IID_PPV_ARGS(m_pipelineStates.at(static_cast<size_t>(Type::kTexCopy)).ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineStates.at(static_cast<size_t>(Type::kTexCopy)).Get()->SetName(Util::getWideStringFromString("bloomTexCopyPipelineState").c_str());
		ThrowIfFailed(result);
	}

	return S_OK;
}
