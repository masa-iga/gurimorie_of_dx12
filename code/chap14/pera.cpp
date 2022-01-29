#include "pera.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "loader.h"
#include "util.h"

using namespace Microsoft::WRL;

static std::vector<float> getGaussianWeights(size_t count, float sigma);
static const std::string kNormalMapImagePath = "../resource/normal/normalmap.jpg";

HRESULT Pera::createResources()
{
	ThrowIfFailed(createVertexBufferResource());
	ThrowIfFailed(createBokehResource());
	ThrowIfFailed(createOffscreenResource());
	ThrowIfFailed(createEffectBufferAndView());

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
		Debug::outputDebugMessage(errBlob.Get());
	}
	ThrowIfFailed(result);

	result = D3DCompileFromFile(
		L"peraPixel.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"peraPs",
		"ps_5_0",
		0,
		0,
		m_ps_bokehH.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
	}
	ThrowIfFailed(result);

	result = D3DCompileFromFile(
		L"peraPixel.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"verticalBokehPs",
		"ps_5_0",
		0,
		0,
		m_ps_bokehV.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
	}
	ThrowIfFailed(result);

	result = D3DCompileFromFile(
		L"peraPixel.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"effectPs",
		"ps_5_0",
		0,
		0,
		m_ps_effect.ReleaseAndGetAddressOf(),
		errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
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
	ThrowIfFalse(m_ps_bokehH != nullptr);

	{
		D3D12_DESCRIPTOR_RANGE range[2] = { };
		{
			range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range[0].BaseShaderRegister = 0;
			range[0].NumDescriptors = 1;
		}
		{
			range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			range[1].BaseShaderRegister = 0;
			range[1].NumDescriptors = 1;
		}

		D3D12_ROOT_PARAMETER rootParameter[2] = { };
		{
			// SRV for reading a texture
			rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParameter[0].DescriptorTable.pDescriptorRanges = &range[0];
			rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		{
			// CBV for Gaussian parameters
			rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
			rootParameter[1].DescriptorTable.pDescriptorRanges = &range[1];
			rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
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
			rsDesc.NumParameters = 2;
			rsDesc.pParameters = rootParameter;
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
			Debug::outputDebugMessage(errBlob.Get());
		}
		ThrowIfFailed(result);

		result = Resource::instance()->getDevice()->CreateRootSignature(
			0,
			rsBlob->GetBufferPointer(),
			rsBlob->GetBufferSize(),
			IID_PPV_ARGS(m_rootSignature_bokeh.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_rootSignature_bokeh.Get()->SetName(Util::getWideStringFromString("peraRootSignatureBokeh").c_str());
		ThrowIfFailed(result);


		// root signature for effect
		{
			// SRV for reading a texture (offscreen buffer)
			range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range[0].BaseShaderRegister = 0;
			range[0].NumDescriptors = 1;
		}
		{
			rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParameter[0].DescriptorTable.pDescriptorRanges = &range[0];
			rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		{
			// SRV for reading a effect texture
			range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range[1].BaseShaderRegister = 1;
			range[1].NumDescriptors = 1;
		}
		{
			rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
			rootParameter[1].DescriptorTable.pDescriptorRanges = &range[1];
			rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
		{
			rsDesc.NumParameters = 2;
			rsDesc.pParameters = rootParameter;
			rsDesc.NumStaticSamplers = 1;
			rsDesc.pStaticSamplers = &sampler;
			rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}

		result = D3D12SerializeRootSignature(
			&rsDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			rsBlob.ReleaseAndGetAddressOf(),
			errBlob.ReleaseAndGetAddressOf());

		if (FAILED(result))
		{
			Debug::outputDebugMessage(errBlob.Get());
		}
		ThrowIfFailed(result);

		result = Resource::instance()->getDevice()->CreateRootSignature(
			0,
			rsBlob->GetBufferPointer(),
			rsBlob->GetBufferSize(),
			IID_PPV_ARGS(m_rootSignature_effect.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_rootSignature_effect.Get()->SetName(Util::getWideStringFromString("peraRootSignatureEffect").c_str());
		ThrowIfFailed(result);
	}

	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = { };
		{
			gpsDesc.pRootSignature = m_rootSignature_bokeh.Get();
			gpsDesc.VS.pShaderBytecode = m_vs.Get()->GetBufferPointer();
			gpsDesc.VS.BytecodeLength = m_vs.Get()->GetBufferSize();
			gpsDesc.PS.pShaderBytecode = m_ps_bokehH.Get()->GetBufferPointer();
			gpsDesc.PS.BytecodeLength = m_ps_bokehH.Get()->GetBufferSize();
			//D3D12_STREAM_OUTPUT_DESC StreamOutput;
			gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			gpsDesc.DepthStencilState.DepthEnable = false;
			gpsDesc.DepthStencilState.StencilEnable = false;
			gpsDesc.InputLayout.pInputElementDescs = layout;
			gpsDesc.InputLayout.NumElements = _countof(layout);
			//D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
			gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			gpsDesc.NumRenderTargets = 2;
			gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			gpsDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
			//DXGI_FORMAT DSVFormat;
			gpsDesc.SampleDesc = { 1, 0 };
			//UINT NodeMask;
			//D3D12_CACHED_PIPELINE_STATE CachedPSO;
			gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		}

		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpsDesc,
			IID_PPV_ARGS(m_pipelineState_bokehH.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineState_bokehH.Get()->SetName(Util::getWideStringFromString("peraPipelineStateBokehH").c_str());
		ThrowIfFailed(result);

		{
			gpsDesc.PS.pShaderBytecode = m_ps_bokehV.Get()->GetBufferPointer();
			gpsDesc.PS.BytecodeLength = m_ps_bokehV.Get()->GetBufferSize();
		}

		result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpsDesc,
			IID_PPV_ARGS(m_pipelineState_bokehV.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineState_bokehV.Get()->SetName(Util::getWideStringFromString("peraPipelineStateBokehV").c_str());
		ThrowIfFailed(result);

		// pipeline state for effect
		{
			gpsDesc.pRootSignature = m_rootSignature_effect.Get();
			gpsDesc.PS.pShaderBytecode = m_ps_effect.Get()->GetBufferPointer();
			gpsDesc.PS.BytecodeLength = m_ps_effect.Get()->GetBufferSize();
		}

		result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&gpsDesc,
			IID_PPV_ARGS(m_pipelineState_effect.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineState_effect.Get()->SetName(Util::getWideStringFromString("peraPipelineStateEffect").c_str());
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Pera::render(const D3D12_CPU_DESCRIPTOR_HANDLE *pRtvHeap, ID3D12DescriptorHeap *pSrvDescHeap)
{
	constexpr bool bBokehMode = true;

	if (bBokehMode)
	{
		ThrowIfFailed(renderBokeh(pRtvHeap, pSrvDescHeap));
	}
	else
	{
		ThrowIfFailed(renderEffect(pRtvHeap, pSrvDescHeap));
	}

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

		result = m_peraVertexBuffer.Get()->SetName(Util::getWideStringFromString("peraVertexBuffer").c_str());
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
	std::vector<float> weights = getGaussianWeights(8, 5.0f);
	ThrowIfFalse(weights.size() > 0);

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
			IID_PPV_ARGS(m_bokehParamBuffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_bokehParamBuffer.Get()->SetName(Util::getWideStringFromString("bokehParamBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		float* pMappedWeight = nullptr;

		auto result = m_bokehParamBuffer.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pMappedWeight));
		ThrowIfFailed(result);
		{
			std::copy(weights.begin(), weights.end(), pMappedWeight);
		}
		m_bokehParamBuffer.Get()->Unmap(0, nullptr);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_cbvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_cbvHeap.Get()->SetName(Util::getWideStringFromString("peraCbvHeap").c_str());
		ThrowIfFailed(result);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbViewDesc = { };
		{
			cbViewDesc.BufferLocation = m_bokehParamBuffer.Get()->GetGPUVirtualAddress();
			cbViewDesc.SizeInBytes = static_cast<UINT>(m_bokehParamBuffer.Get()->GetDesc().Width);
		}

		Resource::instance()->getDevice()->CreateConstantBufferView(
			&cbViewDesc,
			m_cbvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
}

HRESULT Pera::createOffscreenResource()
{
	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resDesc = Resource::instance()->getFrameBuffer(0)->GetDesc();

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			nullptr,
			IID_PPV_ARGS(m_offscreenBuffer.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_offscreenBuffer.Get()->SetName(Util::getWideStringFromString("offscreenBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_offscreenRtvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_offscreenRtvHeap.Get()->SetName(Util::getWideStringFromString("offscreenRtvHeap").c_str());
		ThrowIfFailed(result);
	}

	{
		D3D12_RENDER_TARGET_VIEW_DESC viewDesc = { };
		{
			viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			viewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		}

		Resource::instance()->getDevice()->CreateRenderTargetView(
			m_offscreenBuffer.Get(),
			&viewDesc,
			m_offscreenRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_offscreenSrvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_offscreenSrvHeap.Get()->SetName(Util::getWideStringFromString("offscreenSrvHeap").c_str());
		ThrowIfFailed(result);
	}

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = { };
		{
			viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			viewDesc.Texture2D.MostDetailedMip = 0;
			viewDesc.Texture2D.MipLevels = 1;
			viewDesc.Texture2D.PlaneSlice = 0;
			viewDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		Resource::instance()->getDevice()->CreateShaderResourceView(
			m_offscreenBuffer.Get(),
			&viewDesc,
			m_offscreenSrvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
}

HRESULT Pera::createEffectBufferAndView()
{
	if (FAILED(Loader::instance()->loadImageFromFile(kNormalMapImagePath.c_str(), m_effectTexBuffer)))
		return E_FAIL;

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_effectSrvHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC rsvDesc = { };
		{
			rsvDesc.Format = m_effectTexBuffer.Get()->GetDesc().Format;
			rsvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			rsvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			rsvDesc.Texture2D.MostDetailedMip = 0;
			rsvDesc.Texture2D.MipLevels = 1;
			rsvDesc.Texture2D.PlaneSlice = 0;
			rsvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		};

		Resource::instance()->getDevice()->CreateShaderResourceView(
			m_effectTexBuffer.Get(),
			&rsvDesc,
			m_effectSrvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
}

HRESULT Pera::renderBokeh(const D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHeap, ID3D12DescriptorHeap* pSrvDescHeap)
{
	ThrowIfFalse(pRtvHeap != nullptr);
	ThrowIfFalse(pSrvDescHeap != nullptr);

	ID3D12GraphicsCommandList* commandList = Resource::instance()->getCommandList();

	// first, horizontal bokeh
	commandList->SetGraphicsRootSignature(m_rootSignature_bokeh.Get());
	commandList->SetPipelineState(m_pipelineState_bokehH.Get());

	commandList->SetDescriptorHeaps(1, &pSrvDescHeap);
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
		commandList->SetGraphicsRootDescriptorTable(0, handle);
	}

	commandList->SetDescriptorHeaps(1, m_cbvHeap.GetAddressOf());
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = m_cbvHeap.Get()->GetGPUDescriptorHandleForHeapStart();
		commandList->SetGraphicsRootDescriptorTable(1, handle);
	}

	{
		// render to off screen buffer
		const D3D12_CPU_DESCRIPTOR_HANDLE rtvHeap = m_offscreenRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart();
		commandList->OMSetRenderTargets(1, &rtvHeap, false, nullptr);
	}

	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Config::kWindowWidth), static_cast<float>(Config::kWindowHeight));
		commandList->RSSetViewports(1, &viewport);
	}

	{
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &m_peraVertexBufferView);
	commandList->DrawInstanced(4, 1, 0, 0);

	// vertical bokeh
	commandList->SetPipelineState(m_pipelineState_bokehV.Get()); // to access vertical bokeh shader

	commandList->OMSetRenderTargets(1, pRtvHeap, false, nullptr);

	// read off screen texture which is applied horizontal bokeh
	commandList->SetDescriptorHeaps(1, m_offscreenSrvHeap.GetAddressOf());
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = m_offscreenSrvHeap.Get()->GetGPUDescriptorHandleForHeapStart();
		commandList->SetGraphicsRootDescriptorTable(0, handle);
	}

	commandList->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Pera::renderEffect(const D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHeap, ID3D12DescriptorHeap* pSrvDescHeap)
{
	ThrowIfFalse(pRtvHeap != nullptr);
	ThrowIfFalse(pSrvDescHeap != nullptr);

	ID3D12GraphicsCommandList* commandList = Resource::instance()->getCommandList();

	commandList->SetGraphicsRootSignature(m_rootSignature_effect.Get());
	commandList->SetPipelineState(m_pipelineState_effect.Get());

	commandList->SetDescriptorHeaps(1, &pSrvDescHeap);
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = pSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
		commandList->SetGraphicsRootDescriptorTable(0, handle);
	}
	commandList->SetDescriptorHeaps(1, m_effectSrvHeap.GetAddressOf());
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle = m_effectSrvHeap.Get()->GetGPUDescriptorHandleForHeapStart();
		commandList->SetGraphicsRootDescriptorTable(1, handle);
	}

	commandList->OMSetRenderTargets(1, pRtvHeap, false, nullptr);

	{
		const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Config::kWindowWidth), static_cast<float>(Config::kWindowHeight));
		commandList->RSSetViewports(1, &viewport);
	}

	{
		const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, Config::kWindowWidth, Config::kWindowHeight);
		commandList->RSSetScissorRects(1, &scissorRect);
	}

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	commandList->IASetVertexBuffers(0, 1, &m_peraVertexBufferView);
	commandList->DrawInstanced(4, 1, 0, 0);

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
