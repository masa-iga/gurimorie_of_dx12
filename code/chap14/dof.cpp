#include "dof.h"
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

struct VertexBuffer
{
	DirectX::XMFLOAT3 pos = { };
	DirectX::XMFLOAT2 uv = { };
};

constexpr VertexBuffer vb[] = {
	{{-1.0f, -1.0f, 0.0f}, { 0.0f, 1.0f}},
	{{-1.0f,  1.0f, 0.0f}, { 0.0f, 0.0f}},
	{{ 1.0f, -1.0f, 0.0f}, { 1.0f, 1.0f}},
	{{ 1.0f,  1.0f, 0.0f}, { 1.0f, 0.0f}},
};

HRESULT DoF::init(UINT64 width, UINT height)
{
	ThrowIfFailed(compileShaders());
	ThrowIfFailed(createResource(width, height));
    ThrowIfFailed(createPipelineState());
	return S_OK;
}

HRESULT DoF::render(ID3D12GraphicsCommandList* list, D3D12_CPU_DESCRIPTOR_HANDLE dstRtv, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle, ID3D12DescriptorHeap* pDepthSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE depthSrvHandle)
{
    renderShrink(list, pBaseSrvHeap, baseSrvHandle);

	return S_OK;
}

ComPtr<ID3D12DescriptorHeap> DoF::getWorkDescSrvHeap() const
{
    return m_workDescSrvHeap;
}

D3D12_GPU_DESCRIPTOR_HANDLE DoF::getWorkResourceSrcHandle() const
{
    return m_workDescSrvHeap.Get()->GetGPUDescriptorHandleForHeapStart();
}

HRESULT DoF::compileShaders()
{
    ComPtr<ID3DBlob> errorBlob = nullptr;

    auto result = D3DCompileFromFile(
        kVsFile,
        Constant::kCompileShaderDefines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        kVsEntrypoint,
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

    result = D3DCompileFromFile(
        kPsFile,
        Constant::kCompileShaderDefines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        kPsEntrypoint,
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
	return S_OK;
}

HRESULT DoF::createResource(UINT64 dstWidth, UINT dstHeight)
{
    // Vertex buffer
    {
        const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vb));

        auto result = Resource::instance()->getDevice()->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_vertexResource.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        result = m_vertexResource.Get()->SetName(Util::getWideStringFromString("dofVertexBuffer").c_str());
        ThrowIfFailed(result);

        m_vbView = {
            .BufferLocation = m_vertexResource.Get()->GetGPUVirtualAddress(),
            .SizeInBytes = sizeof(vb),
            .StrideInBytes = sizeof(VertexBuffer),
        };

        VertexBuffer* pVertexBuffer = nullptr;
        result = m_vertexResource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertexBuffer));
        ThrowIfFailed(result);

        std::copy(std::begin(vb), std::end(vb), pVertexBuffer);

        m_vertexResource.Get()->Unmap(0, nullptr);
    }

    // work resource
    {
        {
            const UINT64 width = dstWidth / 2;
            const UINT height = dstHeight;
            constexpr float clearVal[] = { 0.0f, 0.0f, 0.0f, 0.0f };
            const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                Constant::kDefaultRtFormat,
                width,
                height,
                1,
                0,
                1,
                0,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            const D3D12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(resourceDesc.Format, clearVal);

            auto result = Resource::instance()->getDevice()->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                &clearValue,
                IID_PPV_ARGS(m_workResource.ReleaseAndGetAddressOf()));
            ThrowIfFailed(result);

            result = m_workResource.Get()->SetName(Util::getWideStringFromString("dofWorkResource").c_str());
            ThrowIfFailed(result);
        }


        // heap for RTV
        {
            const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                .NumDescriptors = 1,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                .NodeMask = 0,
            };

            auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
                &heapDesc,
                IID_PPV_ARGS(m_workDescRtvHeap.ReleaseAndGetAddressOf()));
            ThrowIfFailed(result);

            result = m_workDescRtvHeap.Get()->SetName(Util::getWideStringFromString("dofWorkRtvHeap").c_str());
            ThrowIfFailed(result);

            const D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
                .Format = m_workResource.Get()->GetDesc().Format,
                .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
                .Texture2D = { 0, 0 },
            };

            Resource::instance()->getDevice()->CreateRenderTargetView(
                m_workResource.Get(),
                &rtvDesc,
                m_workDescRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
        }

        // heap for SRV
        {
            const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
                .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                .NumDescriptors = 1,
                .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                .NodeMask = 0,
            };

            auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
                &heapDesc,
                IID_PPV_ARGS(m_workDescSrvHeap.ReleaseAndGetAddressOf()));
            ThrowIfFailed(result);

            result = m_workDescSrvHeap.Get()->SetName(Util::getWideStringFromString("dofWorkSrvHeap").c_str());
            ThrowIfFailed(result);

            const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
                .Format = Constant::kDefaultRtFormat,
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
                m_workDescSrvHeap.Get()->GetCPUDescriptorHandleForHeapStart());
        }
    }

    return S_OK;
}

HRESULT DoF::createRootSignature()
{
    const D3D12_DESCRIPTOR_RANGE descRanges[] = {
        CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0 /* baseShaderRegister */),
    };

    const D3D12_ROOT_PARAMETER rootParams[] = {
        {
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = CD3DX12_ROOT_DESCRIPTOR_TABLE(_countof(descRanges), descRanges),
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        },
    };

    const D3D12_STATIC_SAMPLER_DESC samplerDescs[] = {
        CD3DX12_STATIC_SAMPLER_DESC(0 /* shaderRegister */, D3D12_FILTER_MIN_MAG_MIP_LINEAR)
    };

	const D3D12_ROOT_SIGNATURE_DESC rootSigDesc = CD3DX12_ROOT_SIGNATURE_DESC(
		_countof(rootParams),
		rootParams,
		_countof(samplerDescs),
		samplerDescs,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> rootSigBlob = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(
		&rootSigDesc,
		Constant::kRootSignatureVersion,
		rootSigBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

    if (FAILED(result))
    {
        Debug::outputDebugMessage(errorBlob.Get());
    }
    ThrowIfFailed(result);

    result = Resource::instance()->getDevice()->CreateRootSignature(
        0 /* nodeMask */,
        rootSigBlob.Get()->GetBufferPointer(),
        rootSigBlob.Get()->GetBufferSize(),
        IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
    ThrowIfFailed(result);

    result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("dofRootSignature").c_str());
    ThrowIfFailed(result);

    return S_OK;
}

HRESULT DoF::createPipelineState()
{
    ThrowIfFailed(createRootSignature());

    constexpr D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {
		    .SemanticName = "POSITION",
		    .SemanticIndex = 0,
			.Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
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
        },
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
        .InputLayout = { inputElementDescs, _countof(inputElementDescs) },
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

    result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("dofPipelineState").c_str());
    ThrowIfFailed(result);

    return S_OK;
}

HRESULT DoF::renderShrink(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* pBaseSrvHeap, D3D12_GPU_DESCRIPTOR_HANDLE baseSrvHandle)
{
    list->SetGraphicsRootSignature(m_rootSignature.Get());
    list->SetPipelineState(m_pipelineState.Get());

#if 0
    list->SetDescriptorHeaps();
    list->SetGraphicsRootDescriptorTable();
#endif
    constexpr int32_t baseWidth = Config::kWindowWidth / 2;
    constexpr int32_t baseHeight = Config::kWindowHeight / 2;

    const D3D12_CPU_DESCRIPTOR_HANDLE dstRtvs[] = { m_workDescRtvHeap.Get()->GetCPUDescriptorHandleForHeapStart() };
    list->OMSetRenderTargets(_countof(dstRtvs), dstRtvs, false, nullptr);

    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    list->IASetVertexBuffers(0, 1, &m_vbView);

    D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(baseWidth), static_cast<float>(baseHeight));
    D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, baseWidth, baseHeight);

    for (uint32_t i = 0; i < 8; ++i)
    {
        list->RSSetViewports(1, &viewport);
        list->RSSetScissorRects(1, &scissorRect);
        list->DrawInstanced(_countof(vb), 1, 0, 0);

        viewport.TopLeftY += viewport.Height;
        viewport.Width /= 2;
        viewport.Height /= 2;

        const auto h = scissorRect.bottom - scissorRect.top;
        scissorRect.top += h;
        scissorRect.bottom += h / 2;
        scissorRect.right /= 2;
    }

    return S_OK;
}
