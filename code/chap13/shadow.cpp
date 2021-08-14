#include "shadow.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

using namespace Microsoft::WRL;

HRESULT Shadow::init()
{
    ThrowIfFailed(compileShaders());
    ThrowIfFailed(createVertexBuffer());
    ThrowIfFailed(createPipelineState());
    return S_OK;
}

HRESULT Shadow::render(const D3D12_CPU_DESCRIPTOR_HANDLE* pRtvHeap, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> texDescHeap)
{
    ID3D12GraphicsCommandList* commandList = Resource::instance()->getCommandList();

    commandList->OMSetRenderTargets(1, pRtvHeap, false, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    commandList->IASetVertexBuffers(0, 1, &m_vbView);

	{
		D3D12_VIEWPORT viewport = { };
		{
			viewport.Width = kWindowWidth;
			viewport.Height = kWindowHeight;
			viewport.TopLeftX = 0;
			viewport.TopLeftY = 0;
			viewport.MaxDepth = 1.0f;
			viewport.MinDepth = 0.0f;
		}
		commandList->RSSetViewports(1, &viewport);
	}

	{
		D3D12_RECT scissorRect = { };
		{
			scissorRect.top = 0;
			scissorRect.left = 0;
			scissorRect.right = scissorRect.left + kWindowWidth;
			scissorRect.bottom = scissorRect.top + kWindowHeight;
		}
		commandList->RSSetScissorRects(1, &scissorRect);
	}

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    commandList->SetDescriptorHeaps(1, texDescHeap.GetAddressOf());
    {
        commandList->SetGraphicsRootDescriptorTable(0, texDescHeap.Get()->GetGPUDescriptorHandleForHeapStart());
    }

    commandList->DrawInstanced(4, 1, 0, 0);

	return S_OK;
}

HRESULT Shadow::compileShaders()
{
	ComPtr<ID3DBlob> errBlob = nullptr;

    auto result = D3DCompileFromFile(
        L"shadowVertex.hlsl",
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
        L"shadowPixel.hlsl",
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

HRESULT Shadow::createVertexBuffer()
{
    struct Vertex
    {
        DirectX::XMFLOAT3 pos = { };
        DirectX::XMFLOAT2 uv = { };
    };

    constexpr Vertex vertex[4] = {
		{{-1.0f, -1.0f, 0.1f }, { 0.0f , 1.0f}}, // lower-left
		{{-1.0f,  1.0f, 0.1f }, { 0.0f , 0.0f}}, // upper-left
		{{ 1.0f, -1.0f, 0.1f }, { 1.0f , 1.0f}}, // lower-right
		{{ 1.0f,  1.0f, 0.1f }, { 1.0f , 0.0f}}, // upper-right
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
            resourceDesc.Width = sizeof(vertex);
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
            IID_PPV_ARGS(m_vbResource.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        result = m_vbResource.Get()->SetName(Util::getWideStringFromString("peraVertexBuffer").c_str());
        ThrowIfFailed(result);
    }

    {
        Vertex* pVertex = nullptr;

        auto result = m_vbResource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertex));
        ThrowIfFailed(result);

        std::copy(std::begin(vertex), std::end(vertex), pVertex);

        m_vbResource.Get()->Unmap(0, nullptr);
    }

    {
		m_vbView.BufferLocation = m_vbResource.Get()->GetGPUVirtualAddress();
		m_vbView.SizeInBytes = sizeof(vertex);
		m_vbView.StrideInBytes = sizeof(Vertex);
    }

    return S_OK;
}

HRESULT Shadow::createPipelineState()
{
	constexpr D3D12_INPUT_ELEMENT_DESC layout[2] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0,
		},
		{
            "TEXCOORD",
            0,
            DXGI_FORMAT_R32G32_FLOAT,
            0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0,
		}
	};

    {
        // t0
        D3D12_DESCRIPTOR_RANGE descRange = { };
		{
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = 1;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = 0;
			descRange.OffsetInDescriptorsFromTableStart = 0;
		}

        D3D12_ROOT_PARAMETER rootParam = { };
		{
			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable.NumDescriptorRanges = 1;
			rootParam.DescriptorTable.pDescriptorRanges = &descRange;
			rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

        D3D12_STATIC_SAMPLER_DESC samplerDesc = { };
		{
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.MipLODBias = 0.0f;
			samplerDesc.MaxAnisotropy = 0;
			samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			samplerDesc.MinLOD = 0.0f;
			samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			samplerDesc.ShaderRegister = 0;
			samplerDesc.RegisterSpace = 0;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

        D3D12_ROOT_SIGNATURE_DESC rsDesc = { };
		{
			rsDesc.NumParameters = 1;
			rsDesc.pParameters = &rootParam;
			rsDesc.NumStaticSamplers = 1;
			rsDesc.pStaticSamplers = &samplerDesc;
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
            outputDebugMessage(errBlob.Get());
            ThrowIfFalse(false);
        }

        result = Resource::instance()->getDevice()->CreateRootSignature(
            0,
            rsBlob.Get()->GetBufferPointer(),
            rsBlob.Get()->GetBufferSize(),
            IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        m_rootSignature.Get()->SetName(Util::getWideStringFromString("shadowRootSignature").c_str());
    }


    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = { };
    {
        pipelineDesc.pRootSignature = m_rootSignature.Get();
        pipelineDesc.VS = { m_vs.Get()->GetBufferPointer(), m_vs.Get()->GetBufferSize() };
        pipelineDesc.PS = { m_ps.Get()->GetBufferPointer(), m_ps.Get()->GetBufferSize() };
        //D3D12_SHADER_BYTECODE DS;
        //D3D12_SHADER_BYTECODE HS;
        //D3D12_SHADER_BYTECODE GS;
        //D3D12_STREAM_OUTPUT_DESC StreamOutput;
        pipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        //D3D12_RASTERIZER_DESC RasterizerState;
        {
            pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            pipelineDesc.RasterizerState.FrontCounterClockwise = false;
            //INT DepthBias;
            //FLOAT DepthBiasClamp;
            //FLOAT SlopeScaledDepthBias;
            pipelineDesc.RasterizerState.DepthClipEnable = false;
            //BOOL MultisampleEnable;
            //BOOL AntialiasedLineEnable;
            //UINT ForcedSampleCount;
            pipelineDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        }
        //D3D12_DEPTH_STENCIL_DESC DepthStencilState;
        {
            pipelineDesc.DepthStencilState.DepthEnable = false;
            //D3D12_DEPTH_WRITE_MASK DepthWriteMask;
            //D3D12_COMPARISON_FUNC DepthFunc;
            pipelineDesc.DepthStencilState.StencilEnable = false;
            //UINT8 StencilReadMask;
            //UINT8 StencilWriteMask;
            //D3D12_DEPTH_STENCILOP_DESC FrontFace;
            //D3D12_DEPTH_STENCILOP_DESC BackFace;
        }
        //D3D12_INPUT_LAYOUT_DESC InputLayout;
        {
            pipelineDesc.InputLayout.pInputElementDescs = layout;
            pipelineDesc.InputLayout.NumElements = _countof(layout);
        }
        //D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
        pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineDesc.NumRenderTargets = 1;
        pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        //DXGI_FORMAT DSVFormat;
        pipelineDesc.SampleDesc = { 1, 0 };
        //UINT NodeMask;
        //D3D12_CACHED_PIPELINE_STATE CachedPSO;
        //D3D12_PIPELINE_STATE_FLAGS Flags;
    }

    auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
        &pipelineDesc,
        IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
    ThrowIfFailed(result);

    result = m_pipelineState.Get()->SetName(Util::getWideStringFromString("shadowPipelineState").c_str());
    ThrowIfFailed(result);

    return S_OK;
}
