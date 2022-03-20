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
    ThrowIfFailed(createDescriptorHeap());
    return S_OK;
}

void Shadow::pushRenderCommand(Type type, ID3D12Resource* resource, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
{
    ThrowIfFalse(resource != nullptr);

    m_commands.at(m_numCommand) = Command(type, resource, viewport, scissorRect);
    ++m_numCommand;
}

HRESULT Shadow::render(ID3D12GraphicsCommandList* pList, D3D12_CPU_DESCRIPTOR_HANDLE dstRt)
{
    setupSrv();

	pList->SetGraphicsRootSignature(m_rootSignature.Get());
	pList->SetDescriptorHeaps(1, m_srvDescHeap.GetAddressOf());
	pList->OMSetRenderTargets(1, &dstRt, false, nullptr);

    for (size_t i = 0; i < m_numCommand; ++i)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE gpuDescHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(
            m_srvDescHeap.Get()->GetGPUDescriptorHandleForHeapStart(),
            i,
            Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

		pList->RSSetViewports(1, &m_commands.at(i).m_viewport);
		pList->RSSetScissorRects(1, &m_commands.at(i).m_scissorRect);

        {
            if (m_commands.at(i).m_type == Type::kQuadR)
            {
                pList->SetPipelineState(m_pipelineStates.at(static_cast<size_t>(Type::kQuadR)).Get());
                pList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootParamIndex::kSrvR), gpuDescHandle);
            }
            else
            {
                pList->SetPipelineState(m_pipelineStates.at(static_cast<size_t>(Type::kQuadRgba)).Get());
                pList->SetGraphicsRootDescriptorTable(static_cast<UINT>(RootParamIndex::kSrvRgba), gpuDescHandle);
            }

            pList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
            pList->IASetVertexBuffers(0, 1, &m_vbViews.at(static_cast<size_t>(MeshType::kQuad)));

            pList->DrawInstanced(4, 1, 0, 0);
        }

        // draw frame
        {
            pList->SetPipelineState(m_pipelineStates.at(static_cast<size_t>(Type::kFrameLine)).Get());
            pList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
            pList->IASetVertexBuffers(0, 1, &m_vbViews.at(static_cast<size_t>(MeshType::kFrameLine)));

            pList->DrawInstanced(5, 1, 0, 0);
        }
    }

    m_numCommand = 0;

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
        m_commonVs.ReleaseAndGetAddressOf(),
        errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
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
        m_psArray.at(static_cast<size_t>(Type::kQuadR)).ReleaseAndGetAddressOf(),
        errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
        ThrowIfFalse(false);
	}

    result = D3DCompileFromFile(
        L"shadowPixel.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "mainRgba",
        "ps_5_0",
        0,
        0,
        m_psArray.at(static_cast<size_t>(Type::kQuadRgba)).ReleaseAndGetAddressOf(),
        errBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errBlob.Get());
        ThrowIfFalse(false);
	}

    result = D3DCompileFromFile(
        L"shadowPixel.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "black",
        "ps_5_0",
        0,
        0,
        m_psArray.at(static_cast<size_t>(Type::kFrameLine)).ReleaseAndGetAddressOf(),
        errBlob.ReleaseAndGetAddressOf());

    if (FAILED(result))
    {
        Debug::outputDebugMessage(errBlob.Get());
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

    constexpr Vertex quadTriangleVertecies[4] = {
		{{-1.0f, -1.0f, 0.1f }, { 0.0f , 1.0f}}, // lower-left
		{{-1.0f,  1.0f, 0.1f }, { 0.0f , 0.0f}}, // upper-left
		{{ 1.0f, -1.0f, 0.1f }, { 1.0f , 1.0f}}, // lower-right
		{{ 1.0f,  1.0f, 0.1f }, { 1.0f , 0.0f}}, // upper-right
    };

    constexpr Vertex frameLineVertecies[] = {
        {{-0.999f, -0.999f, 0.0f }, { 0.0f , 1.0f}}, // lower-left
        {{-0.999f,  0.999f, 0.0f }, { 0.0f , 0.0f}}, // upper-left
        {{ 0.999f,  0.999f, 0.0f }, { 1.0f , 0.0f}}, // upper-right
        {{ 0.999f, -0.999f, 0.0f }, { 1.0f , 1.0f}}, // lower-right
        {{-0.999f, -0.999f, 0.0f }, { 0.0f , 1.0f}}, // lower-left
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
            resourceDesc.Width = sizeof(quadTriangleVertecies);
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc = { 1, 0 };
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        }

		auto& resource = m_vbResources.at(static_cast<size_t>(MeshType::kQuad));

        {
            auto result = Resource::instance()->getDevice()->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
            ThrowIfFailed(result);

            result = resource.Get()->SetName(Util::getWideStringFromString("shadowVertexQuadTriangleBuffer").c_str());
            ThrowIfFailed(result);
        }

		{
			Vertex* pVertex = nullptr;

			auto result = resource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertex));
			ThrowIfFailed(result);

			std::copy(std::begin(quadTriangleVertecies), std::end(quadTriangleVertecies), pVertex);

			resource.Get()->Unmap(0, nullptr);
		}
		{
			D3D12_VERTEX_BUFFER_VIEW& vbView = m_vbViews.at(static_cast<size_t>(MeshType::kQuad));

			vbView.BufferLocation = resource.Get()->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(quadTriangleVertecies);
			vbView.StrideInBytes = sizeof(Vertex);
		}
    }


    {
        D3D12_RESOURCE_DESC resourceDesc = m_vbResources.at(static_cast<size_t>(MeshType::kQuad)).Get()->GetDesc();
		resourceDesc.Width = sizeof(frameLineVertecies);

        D3D12_HEAP_PROPERTIES heapProp = { };
        {
            auto result = m_vbResources.at(static_cast<size_t>(MeshType::kQuad)).Get()->GetHeapProperties(&heapProp, nullptr);
            ThrowIfFailed(result);
        }

		auto& resource = m_vbResources.at(static_cast<size_t>(MeshType::kFrameLine));

        {
            auto result = Resource::instance()->getDevice()->CreateCommittedResource(
                &heapProp,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
            ThrowIfFailed(result);

            result = resource.Get()->SetName(Util::getWideStringFromString("shadowVertexFrameLineBuffer").c_str());
            ThrowIfFailed(result);
        }

		{
			Vertex* pVertex = nullptr;

			auto result = resource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertex));
			ThrowIfFailed(result);

			std::copy(std::begin(frameLineVertecies), std::end(frameLineVertecies), pVertex);

			resource.Get()->Unmap(0, nullptr);
		}
		{
            D3D12_VERTEX_BUFFER_VIEW& vbView = m_vbViews.at(static_cast<size_t>(MeshType::kFrameLine));

			vbView.BufferLocation = resource.Get()->GetGPUVirtualAddress();
			vbView.SizeInBytes = sizeof(frameLineVertecies);
			vbView.StrideInBytes = sizeof(Vertex);
		}
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
        D3D12_DESCRIPTOR_RANGE descRanges[2] = { };
		{
			// t0
			descRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRanges[0].NumDescriptors = 1;
			descRanges[0].BaseShaderRegister = 0;
			descRanges[0].RegisterSpace = 0;
			descRanges[0].OffsetInDescriptorsFromTableStart = 0;
		}
		{
			// t1
			descRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRanges[1].NumDescriptors = 1;
			descRanges[1].BaseShaderRegister = 1;
			descRanges[1].RegisterSpace = 0;
			descRanges[1].OffsetInDescriptorsFromTableStart = 0;
		}

        D3D12_ROOT_PARAMETER rootParams[2] = { };
		{
			rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParams[0].DescriptorTable.pDescriptorRanges = &descRanges[0];
			rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}
        {
			rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
			rootParams[1].DescriptorTable.pDescriptorRanges = &descRanges[1];
			rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
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
			rsDesc.NumParameters = 2;
			rsDesc.pParameters = rootParams;
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
            Debug::outputDebugMessage(errBlob.Get());
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
        pipelineDesc.VS = { m_commonVs.Get()->GetBufferPointer(), m_commonVs.Get()->GetBufferSize() };
        pipelineDesc.PS = { m_psArray.at(static_cast<size_t>(Type::kQuadR)).Get()->GetBufferPointer(), m_psArray.at(static_cast<size_t>(Type::kQuadR)).Get()->GetBufferSize() };
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
    {
        auto& pipelineState = m_pipelineStates.at(static_cast<size_t>(Type::kQuadR));

        auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
            &pipelineDesc,
            IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        result = pipelineState.Get()->SetName(Util::getWideStringFromString("shadowRPipelineState").c_str());
        ThrowIfFailed(result);
    }

    {
		pipelineDesc.PS = { m_psArray.at(static_cast<size_t>(Type::kQuadRgba)).Get()->GetBufferPointer(), m_psArray.at(static_cast<size_t>(Type::kQuadRgba)).Get()->GetBufferSize() };
    }
    {
        auto& pipelineState = m_pipelineStates.at(static_cast<size_t>(Type::kQuadRgba));

        auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
            &pipelineDesc,
            IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        result = pipelineState.Get()->SetName(Util::getWideStringFromString("shadowRgbaPipelineState").c_str());
        ThrowIfFailed(result);
    }

    {
		pipelineDesc.PS = { m_psArray.at(static_cast<size_t>(Type::kFrameLine)).Get()->GetBufferPointer(), m_psArray.at(static_cast<size_t>(Type::kFrameLine)).Get()->GetBufferSize() };
        pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    }
    {
        auto& pipelineState = m_pipelineStates.at(static_cast<size_t>(Type::kFrameLine));

        auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
            &pipelineDesc,
            IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
        ThrowIfFailed(result);

        result = pipelineState.Get()->SetName(Util::getWideStringFromString("shadowLinePipelineState").c_str());
        ThrowIfFailed(result);
    }

    return S_OK;
}

HRESULT Shadow::createDescriptorHeap()
{
    const D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
		.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		.NumDescriptors = kMaxNumDescriptor,
		.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
		.NodeMask = 0,
    };

	auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
		&heapDesc,
		IID_PPV_ARGS(m_srvDescHeap.ReleaseAndGetAddressOf()));
    ThrowIfFailed(result);

    result = m_srvDescHeap.Get()->SetName(Util::getWideStringFromString("shadowDescHeap").c_str());
    ThrowIfFailed(result);

    return S_OK;
}

void Shadow::setupSrv()
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = m_srvDescHeap.Get()->GetCPUDescriptorHandleForHeapStart();

    for (size_t i = 0; i < m_numCommand; ++i)
    {
        auto resource = m_commands.at(i).m_resource;

        DXGI_FORMAT format = resource->GetDesc().Format;

        if (format == DXGI_FORMAT_R32_TYPELESS)
        {
            format = DXGI_FORMAT_R32_FLOAT;
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
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
