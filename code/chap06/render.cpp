#include "render.h"
#include <cassert>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <synchapi.h>
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

struct Vertex
{
	DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT2 uv = { 0.0f, 0.0f };
};

static HRESULT createRootSignature(ID3D12RootSignature** ppRootSignature);
static HRESULT setViewportScissor();
static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence);
static void outputDebugMessage(ID3DBlob* errorBlob);
static HRESULT createConstantBuffer();

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));
	ThrowIfFailed(createVertexBuffer());
	ThrowIfFailed(loadShaders());
	ThrowIfFailed(loadImage());

	constexpr bool bUseCopyTextureRegion = true;
	if (bUseCopyTextureRegion)
	{
		ThrowIfFailed(createTextureBuffer2());
	}
	else
	{
		ThrowIfFailed(createTextureBuffer());
	}

	ThrowIfFailed(createConstantBuffer());
	ThrowIfFailed(createViews());
	ThrowIfFailed(createPipelineState());

	return S_OK;
}

HRESULT Render::render()
{
	const UINT bbIdx = getInstanceOfSwapChain()->GetCurrentBackBufferIndex();

	// resource barrier
	D3D12_RESOURCE_BARRIER barrier = { };
	{
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = getBackBuffer(bbIdx);
		barrier.Transition.Subresource = 0;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}
	getInstanceOfCommandList()->ResourceBarrier(1, &barrier);


	// link swap chain's memory to output merger
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * static_cast<SIZE_T>(getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

	getInstanceOfCommandList()->OMSetRenderTargets(1, &rtvH, true, nullptr);


	// clear render target
	constexpr float clearColor[] = { 0.05f, 0.05f, 0.4f, 1.0f };
	getInstanceOfCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


	// draw triangle
	ThrowIfFalse(m_pipelineState != nullptr);
	getInstanceOfCommandList()->SetPipelineState(m_pipelineState);

	ThrowIfFalse(m_rootSignature != nullptr);
	getInstanceOfCommandList()->SetGraphicsRootSignature(m_rootSignature);

	ThrowIfFalse(m_texDescHeap != nullptr);
	getInstanceOfCommandList()->SetDescriptorHeaps(1, &m_texDescHeap);
	getInstanceOfCommandList()->SetGraphicsRootDescriptorTable(
		0,
		m_texDescHeap->GetGPUDescriptorHandleForHeapStart());

	setViewportScissor();
	getInstanceOfCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	getInstanceOfCommandList()->IASetVertexBuffers(0, 1, &m_vbView);
	getInstanceOfCommandList()->IASetIndexBuffer(&m_ibView);
	getInstanceOfCommandList()->DrawIndexedInstanced(6, 1, 0, 0, 0);


	// resource barrier
	{
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	}
	getInstanceOfCommandList()->ResourceBarrier(1, &barrier);


	// execute command lists
	ThrowIfFailed(getInstanceOfCommandList()->Close());

	ID3D12CommandList* cmdLists[] = { getInstanceOfCommandList() };

	getInstanceOfCommandQueue()->ExecuteCommandLists(1, cmdLists);

	// send signal
	ThrowIfFailed(getInstanceOfCommandQueue()->Signal(m_pFence, ++m_fenceVal));

	return S_OK;
}

HRESULT Render::waitForEndOfRendering()
{
	while (m_pFence->GetCompletedValue() != m_fenceVal)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		ThrowIfFailed(m_pFence->SetEventOnCompletion(m_fenceVal, event));

		auto ret = WaitForSingleObject(event, INFINITE);
		ThrowIfFalse(ret == WAIT_OBJECT_0);

		BOOL ret2 = CloseHandle(event); // fail‚·‚é

		if (!ret2)
		{
			DebugOutputFormatString("failed to close handle. (ret %d error %d)\n", ret2, GetLastError());
			ThrowIfFalse(FALSE);
		}
	}

	// reset command allocator & list
	ThrowIfFailed(getInstanceOfCommandAllocator()->Reset());
	ThrowIfFailed(getInstanceOfCommandList()->Reset(getInstanceOfCommandAllocator(), nullptr));

	return S_OK;
}

HRESULT Render::swap()
{
	ThrowIfFailed(getInstanceOfSwapChain()->Present(1, 0));

	return S_OK;
}

HRESULT Render::loadShaders()
{
	ID3DBlob* errorBlob = nullptr;

	auto ret = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&m_vsBlob,
		&errorBlob
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob);
	}
	ThrowIfFailed(ret);


	ret = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPs",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&m_psBlob,
		&errorBlob
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob);
	}
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::loadImage()
{
	using namespace DirectX;

	auto ret = LoadFromWICFile(
		L"img/textest.png",
		DirectX::WIC_FLAGS_NONE,
		&m_metadata,
		m_scratchImage);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::createPipelineState()
{
	D3D12_INPUT_ELEMENT_DESC elementDescs[] =
	{
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
		},
	};

	ThrowIfFailed(createRootSignature(&m_rootSignature));
	ThrowIfFalse(m_rootSignature != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeDesc = { };
	{
		gpipeDesc.pRootSignature = m_rootSignature;
		gpipeDesc.VS = { m_vsBlob->GetBufferPointer(), m_vsBlob->GetBufferSize() };
		gpipeDesc.PS = { m_psBlob->GetBufferPointer(), m_psBlob->GetBufferSize() };
		// D3D12_SHADER_BYTECODE gpipeDesc.DS;
		// D3D12_SHADER_BYTECODE gpipeDesc.HS;
		// D3D12_SHADER_BYTECODE gpipeDesc.GS;
		// D3D12_STREAM_OUTPUT_DESC StreamOutput;
		gpipeDesc.BlendState.AlphaToCoverageEnable = false;
		gpipeDesc.BlendState.IndependentBlendEnable = false;
		gpipeDesc.BlendState.RenderTarget[0].BlendEnable = false;
		gpipeDesc.BlendState.RenderTarget[0].LogicOpEnable = false;
		gpipeDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		gpipeDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		gpipeDesc.RasterizerState = {
			D3D12_FILL_MODE_SOLID,
			D3D12_CULL_MODE_NONE,
			true /* DepthClipEnable */,
			false /* MultisampleEnable */
		};
		// D3D12_DEPTH_STENCIL_DESC DepthStencilState;
		gpipeDesc.InputLayout = {
			elementDescs,
			_countof(elementDescs)
		};
		gpipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		gpipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpipeDesc.NumRenderTargets = 1;
		gpipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		// DXGI_FORMAT DSVFormat;
		gpipeDesc.SampleDesc = {
			1 /* count */,
			0 /* quality */
		};
		// UINT NodeMask;
		// D3D12_CACHED_PIPELINE_STATE CachedPSO;
		// D3D12_PIPELINE_STATE_FLAGS Flags;
	}

	auto ret = getInstanceOfDevice()->CreateGraphicsPipelineState(&gpipeDesc, IID_PPV_ARGS(&m_pipelineState));
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::createVertexBuffer()
{
	const Vertex vertices[] = {
		{{-0.5f, -0.7f, 0.0f}, {0.0f, 1.0f}},
		{{-0.5f,  0.7f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.7f, 0.0f}, {1.0f, 1.0f}},
		{{ 0.5f,  0.7f, 0.0f}, {1.0f, 0.0f}},
	};

	const uint16_t indices[] = {
		0, 1, 2,
		2, 1, 3
	};

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
		resourceDesc.Width = sizeof(vertices);
		resourceDesc.Height = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	ID3D12Resource* vertBuff = nullptr;

	// create both a resrouce and an implicit heap
	auto ret = getInstanceOfDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));
	ThrowIfFailed(ret);


	// copy vertices to the allocated resource memory
	Vertex* vertMap = nullptr;

	ret = vertBuff->Map(
		0,
		nullptr,
		(void**)&vertMap);
	ThrowIfFailed(ret);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);

	// create vertex buffer view
	{
		m_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
		m_vbView.SizeInBytes = sizeof(vertices);
		m_vbView.StrideInBytes = sizeof(vertices[0]);
	}


	// create a resource for index buffer
	ID3D12Resource* idxBuff = nullptr;

	resourceDesc.Width = sizeof(indices);

	ret = getInstanceOfDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff));
	ThrowIfFailed(ret);


	uint16_t* mappedIdx = nullptr;

	ret = idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	ThrowIfFailed(ret);

	std::copy(std::begin(indices), std::end(indices), mappedIdx);

	idxBuff->Unmap(0, nullptr);

	// create index buffer view
	{
		m_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
		m_ibView.Format = DXGI_FORMAT_R16_UINT;
		m_ibView.SizeInBytes = sizeof(indices);
	}

	return S_OK;
}

HRESULT Render::createTextureBuffer()
{
	// create a heap for texture
	D3D12_HEAP_PROPERTIES heapProp = { };
	{
		heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;
	}

	D3D12_RESOURCE_DESC resDesc = {};
	{
		resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_metadata.dimension);
		resDesc.Alignment = 0;
		resDesc.Width = m_metadata.width;
		resDesc.Height = static_cast<UINT>(m_metadata.height);
		resDesc.DepthOrArraySize = static_cast<UINT16>(m_metadata.arraySize);
		resDesc.MipLevels = static_cast<UINT16>(m_metadata.mipLevels);
		resDesc.Format = m_metadata.format;
		resDesc.SampleDesc = { 1, 0 };
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	auto ret = getInstanceOfDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_texResource));
	ThrowIfFailed(ret);

	const auto img = m_scratchImage.GetImage(0, 0, 0);
	ThrowIfFalse(img != nullptr);

	// upload texture to device
	ret = m_texResource->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		static_cast<UINT>(img->rowPitch),
		static_cast<UINT>(img->slicePitch)
	);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT Render::createTextureBuffer2()
{
	// create a resource for uploading
	const auto img = m_scratchImage.GetImage(0, 0, 0);
	ThrowIfFalse(img != nullptr);

	ID3D12Resource* texUploadBuff = nullptr;
	{
		D3D12_HEAP_PROPERTIES uploadHeapProp = { };
		{
			uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD; // to be able to map
			uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			uploadHeapProp.CreationNodeMask = 0;
			uploadHeapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resDesc = { };
		{

			resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resDesc.Alignment = 0;
			resDesc.Width = img->slicePitch;
			resDesc.Height = 1;
			resDesc.DepthOrArraySize = 1;
			resDesc.MipLevels = 1;
			resDesc.Format = DXGI_FORMAT_UNKNOWN; // this is chunk of data, so should be unknown
			resDesc.SampleDesc = { 1, 0 };
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = getInstanceOfDevice()->CreateCommittedResource(
			&uploadHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, // CPU writable and GPU redable
			nullptr,
			IID_PPV_ARGS(&texUploadBuff)
		);
		ThrowIfFailed(result);
	}


	// map
	{
		uint8_t* mapForImg = nullptr;

		auto result = texUploadBuff->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mapForImg)
		);
		ThrowIfFailed(result);

		std::copy_n(
			img->pixels,
			img->slicePitch,
			mapForImg);

		texUploadBuff->Unmap(0, nullptr);
	}


	// create a resource for reading from GPU
	{
		D3D12_HEAP_PROPERTIES texHeapProp = { };
		{
			texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			texHeapProp.CreationNodeMask = 0;
			texHeapProp.VisibleNodeMask = 0;
		}

		D3D12_RESOURCE_DESC resDesc = { };
		{
			resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_metadata.dimension);
			resDesc.Alignment = 0;
			resDesc.Width = m_metadata.width;
			resDesc.Height = static_cast<UINT>(m_metadata.height);
			resDesc.DepthOrArraySize = static_cast<UINT16>(m_metadata.arraySize);
			resDesc.MipLevels = static_cast<UINT16>(m_metadata.mipLevels);
			resDesc.Format = m_metadata.format;
			resDesc.SampleDesc = { 1, 0 };
			resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = getInstanceOfDevice()->CreateCommittedResource(
			&texHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texResource)
		);
		ThrowIfFailed(result);
	}


	// copy texture
	D3D12_TEXTURE_COPY_LOCATION src = { };
	{
		src.pResource = texUploadBuff;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint.Offset = 0;
		src.PlacedFootprint.Footprint.Format = img->format;
		src.PlacedFootprint.Footprint.Width = static_cast<UINT>(m_metadata.width);
		src.PlacedFootprint.Footprint.Height = static_cast<UINT>(m_metadata.height);
		src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(m_metadata.depth);
		src.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(img->rowPitch);
	}
	ThrowIfFalse(src.PlacedFootprint.Footprint.RowPitch % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT == 0);
	D3D12_TEXTURE_COPY_LOCATION dst = { };
	{
		dst.pResource = m_texResource;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
	}
	getInstanceOfCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);


	// barrier
	D3D12_RESOURCE_BARRIER barrierDesc = { };
	{
		barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrierDesc.Transition.pResource = m_texResource;
		barrierDesc.Transition.Subresource = 0;
		barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
	getInstanceOfCommandList()->ResourceBarrier(1, &barrierDesc);
	ThrowIfFailed(getInstanceOfCommandList()->Close());


	ID3D12CommandList* cmdLists[] = { getInstanceOfCommandList() };
	getInstanceOfCommandQueue()->ExecuteCommandLists(1, cmdLists);

	// wait until the copy is done
	ID3D12Fence* fence = nullptr;
	ThrowIfFailed(createFence(0, &fence));
	ThrowIfFailed(getInstanceOfCommandQueue()->Signal(fence, 1));

	if (fence->GetCompletedValue() != 1)
	{
		HANDLE event = CreateEvent(nullptr, false, false, nullptr);

		ThrowIfFailed(fence->SetEventOnCompletion(1, event));

		auto ret = WaitForSingleObject(event, INFINITE);
		ThrowIfFalse(ret == WAIT_OBJECT_0);

		BOOL ret2 = CloseHandle(event); // fail‚·‚é

		if (!ret2)
		{
			DebugOutputFormatString("failed to close handle. (ret %d error %d)\n", ret2, GetLastError());
			ThrowIfFalse(FALSE);
		}
	}

	// reset command allocator & list
	ThrowIfFailed(getInstanceOfCommandAllocator()->Reset());
	ThrowIfFailed(getInstanceOfCommandList()->Reset(getInstanceOfCommandAllocator(), nullptr));

	return S_OK;
}

HRESULT Render::createViews()
{
	// create a descriptor heap for shader resource
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = { };
	{
		descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		descHeapDesc.NumDescriptors = 1;
		descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		descHeapDesc.NodeMask = 0;
	}
	auto ret = getInstanceOfDevice()->CreateDescriptorHeap(
		&descHeapDesc,
		IID_PPV_ARGS(&m_texDescHeap));
	ThrowIfFailed(ret);

	// create a shader resource view on the heap
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
	{
		srvDesc.Format = m_metadata.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.PlaneSlice = 0;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	getInstanceOfDevice()->CreateShaderResourceView(
		m_texResource,
		&srvDesc,
		m_texDescHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return S_OK;
}

static HRESULT createRootSignature(ID3D12RootSignature** ppRootSignature)
{
	ThrowIfFalse(ppRootSignature != nullptr);

	// need to input vertex
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	{
		// create descriptor table to bind a texture
		D3D12_ROOT_PARAMETER rootParam = { };
		{
			D3D12_DESCRIPTOR_RANGE descTblRange = { };
			{
				descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				descTblRange.NumDescriptors = 1;
				descTblRange.BaseShaderRegister = 0;
				descTblRange.RegisterSpace = 0;
				descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable.NumDescriptorRanges = 1;
			rootParam.DescriptorTable.pDescriptorRanges = &descTblRange;
			rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		}

		D3D12_STATIC_SAMPLER_DESC samplerDesc = { };
		{
			samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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

		rootSignatureDesc.NumParameters = 1; // for texture
		rootSignatureDesc.pParameters = &rootParam;
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &samplerDesc;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ID3DBlob* rootSigBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	auto ret = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob);
	}
	ThrowIfFailed(ret);

	ret = getInstanceOfDevice()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(ppRootSignature)
	);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT setViewportScissor()
{
	D3D12_VIEWPORT viewport = { };
	{
		viewport.Width = kWindowWidth;
		viewport.Height = kWindowHeight;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MaxDepth = 1.0f;
		viewport.MaxDepth = 0.0f;
	}

	getInstanceOfCommandList()->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = { };
	{
		scissorRect.top = 0;
		scissorRect.left = 0;
		scissorRect.right = scissorRect.left + kWindowWidth;
		scissorRect.bottom = scissorRect.top + kWindowHeight;
	}

	getInstanceOfCommandList()->RSSetScissorRects(1, &scissorRect);

	return S_OK;
}

static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence)
{
	return getInstanceOfDevice()->CreateFence(
		initVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(ppFence));
}

static void outputDebugMessage(ID3DBlob* errorBlob)
{
	if (errorBlob == nullptr)
		return;

	std::string errStr;
	errStr.resize(errorBlob->GetBufferSize());

	std::copy_n(
		static_cast<char*>(errorBlob->GetBufferPointer()),
		errorBlob->GetBufferSize(),
		errStr.begin());
	errStr += "\n";

	OutputDebugStringA(errStr.c_str());
}

static HRESULT createConstantBuffer()
{
	using namespace DirectX;

	XMMATRIX matrix = DirectX::XMMatrixIdentity();
	const size_t w = Util::alignmentedSize(sizeof(matrix), 256);

	ID3D12Resource* constBuff = nullptr;
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
			resourceDesc.Width = w;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto result = getInstanceOfDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constBuff)
		);
		ThrowIfFailed(result);
	}

	XMMATRIX* mapMatrix = nullptr;
	{
		auto result = constBuff->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mapMatrix)
		);
		ThrowIfFailed(result);
	}
	*mapMatrix = matrix;


	ID3D12DescriptorHeap* descHeap = nullptr;
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto result = getInstanceOfDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(&descHeap)
		);
		ThrowIfFailed(result);
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC bvDesc = { };
	{
		bvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
		bvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	}

	auto descHandle = descHeap->GetCPUDescriptorHandleForHeapStart();
	//descHandle.ptr += getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	getInstanceOfDevice()->CreateConstantBufferView(
		&bvDesc,
		descHandle
	);

	// TODO: merge desc heap to SRV's one ?

	return S_OK;
}
