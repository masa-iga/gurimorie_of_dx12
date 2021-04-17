#include "render.h"
#include <cassert>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <synchapi.h>
#include "config.h"
#include "debug.h"
#include "init.h"
#include "pmd_reader.h"
#include "util.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

#define DISABLE_MATRIX (0)

static HRESULT setupRootSignature(ID3D12RootSignature** ppRootSignature);
static HRESULT setViewportScissor();
static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence);
static HRESULT createDepthBuffer(ID3D12Resource** ppResource, ID3D12DescriptorHeap** ppDescHeap);
static void outputDebugMessage(ID3DBlob* errorBlob);

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));
	ThrowIfFailed(m_pmdReader.readData());
	ThrowIfFailed(m_pmdReader.createResources());
	ThrowIfFailed(createDepthBuffer(&m_depthResource, &m_dsvHeap));
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

	ThrowIfFailed(createSceneMatrixBuffer());
	ThrowIfFailed(createViews());
	ThrowIfFailed(createPipelineState());

	return S_OK;
}

HRESULT Render::render()
{
	updateMatrix();


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


	// link swap chain's and depth buffers to output merger
	D3D12_CPU_DESCRIPTOR_HANDLE rtvH = getRtvHeaps()->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * static_cast<SIZE_T>(getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));

	D3D12_CPU_DESCRIPTOR_HANDLE dsvH = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();

	getInstanceOfCommandList()->OMSetRenderTargets(1, &rtvH, false, &dsvH);

	// clear render target
	constexpr float clearColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	getInstanceOfCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// clear depth buffer
	getInstanceOfCommandList()->ClearDepthStencilView(
		dsvH,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
        nullptr);

	ThrowIfFalse(m_pipelineState != nullptr);
	getInstanceOfCommandList()->SetPipelineState(m_pipelineState);

	ThrowIfFalse(m_rootSignature != nullptr);
	getInstanceOfCommandList()->SetGraphicsRootSignature(m_rootSignature);

	setViewportScissor();
	getInstanceOfCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	getInstanceOfCommandList()->IASetVertexBuffers(0, 1, m_pmdReader.getVbView());
	getInstanceOfCommandList()->IASetIndexBuffer(m_pmdReader.getIbView());

	// bind MVP matrix
	{
		ThrowIfFalse(m_basicDescHeap != nullptr);
		getInstanceOfCommandList()->SetDescriptorHeaps(1, &m_basicDescHeap);
		getInstanceOfCommandList()->SetGraphicsRootDescriptorTable(
			0, // bind to b0
			m_basicDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind material & draw
	{
		auto const materialDescHeap = m_pmdReader.getMaterialDescHeap();
		getInstanceOfCommandList()->SetDescriptorHeaps(1, &materialDescHeap);

		const auto cbvSrvIncSize = getInstanceOfDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 4;
		auto materialH = materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		UINT indexOffset = 0;

		for (const auto& m : m_pmdReader.getMaterials())
		{
			getInstanceOfCommandList()->SetGraphicsRootDescriptorTable(
				1, // bind to b1
				materialH);

			getInstanceOfCommandList()->DrawIndexedInstanced(m.indicesNum, 1, indexOffset, 0, 0);

			materialH.ptr += cbvSrvIncSize;
			indexOffset += m.indicesNum;
		}
	}

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
	ThrowIfFailed(setupRootSignature(&m_rootSignature));
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
		gpipeDesc.DepthStencilState.DepthEnable = true;
		gpipeDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		gpipeDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		gpipeDesc.DepthStencilState.StencilEnable = false;
		//gpipeDesc.DepthStencilState.StencilReadMask = 0;
		//gpipeDesc.DepthStencilState.StencilWriteMask = 0;
		//D3D12_DEPTH_STENCILOP_DESC FrontFace;
		//D3D12_DEPTH_STENCILOP_DESC BackFace;
		auto [elementDescs, numOfElement] = m_pmdReader.getInputElementDesc();
		gpipeDesc.InputLayout = {
			elementDescs,
			numOfElement
		};
		gpipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		gpipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpipeDesc.NumRenderTargets = 1;
		gpipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		gpipeDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
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

HRESULT Render::createSceneMatrixBuffer()
{
	using namespace DirectX;

	{
		const size_t w = Util::alignmentedSize(sizeof(SceneMatrix), 256);

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
			IID_PPV_ARGS(&m_mvpMatrixResource)
		);
		ThrowIfFailed(result);
	}

	{
		auto result = m_mvpMatrixResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&m_sceneMatrix)
		);
		ThrowIfFailed(result);
	}

	return S_OK;
}

HRESULT Render::createViews()
{
	// create a descriptor heap for shader resource
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = { };
	{
		{
			descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			descHeapDesc.NumDescriptors = 1;
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			descHeapDesc.NodeMask = 0;
		}

		auto ret = getInstanceOfDevice()->CreateDescriptorHeap(
			&descHeapDesc,
			IID_PPV_ARGS(&m_basicDescHeap));
		ThrowIfFailed(ret);
	}

	// create a shader resource view on the heap
	auto basicHeapHandle = m_basicDescHeap->GetCPUDescriptorHandleForHeapStart();
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC bvDesc = { };
		{
			bvDesc.BufferLocation = m_mvpMatrixResource->GetGPUVirtualAddress();
			bvDesc.SizeInBytes = static_cast<UINT>(m_mvpMatrixResource->GetDesc().Width);
		}
		getInstanceOfDevice()->CreateConstantBufferView(
			&bvDesc,
			basicHeapHandle
		);
	}

	return S_OK;
}

HRESULT Render::updateMatrix()
{
	using namespace DirectX;

	static float angle = 0.0f;
	const auto worldMat = DirectX::XMMatrixRotationY(angle);

	constexpr XMFLOAT3 eye(0, 15, -15);
	constexpr XMFLOAT3 target(0, 15, 0);
	constexpr XMFLOAT3 up(0, 1, 0);

	const auto viewMat = XMMatrixLookAtLH(
		DirectX::XMLoadFloat3(&eye),
		DirectX::XMLoadFloat3(&target),
		DirectX::XMLoadFloat3(&up)
	);

	auto projMat = DirectX::XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(kWindowWidth) / static_cast<float>(kWindowHeight),
		1.0f,
		100.0f
	);

#if DISABLE_MATRIX
	m_sceneMatrix->world = XMMatrixIdentity();
	m_sceneMatrix->view = XMMatrixIdentity();
	m_sceneMatrix->proj = XMMatrixIdentity();
	m_sceneMatrix->eye = XMFLOAT3(0.0f, 0.0f, 0.0f);
#else
	m_sceneMatrix->world = worldMat;
	m_sceneMatrix->view = viewMat;
	m_sceneMatrix->proj = projMat;
	m_sceneMatrix->eye = eye;
#endif // DISABLE_MATRIX

	angle += 0.02f;

	return S_OK;
}

static HRESULT setupRootSignature(ID3D12RootSignature** ppRootSignature)
{
	ThrowIfFalse(ppRootSignature != nullptr);

	// need to input vertex
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	{
		// create descriptor table to bind resources (e.g. texture, constant buffer, etc.)
		D3D12_ROOT_PARAMETER rootParam[2] = { };
		{
			D3D12_DESCRIPTOR_RANGE descTblRange[3] = { };
			{
				// MVP matrix
				descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				descTblRange[0].NumDescriptors = 1;
				descTblRange[0].BaseShaderRegister = 0; // b0
				descTblRange[0].RegisterSpace = 0;
				descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				// material
				descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				descTblRange[1].NumDescriptors = 1;
				descTblRange[1].BaseShaderRegister = 1; // b1
				descTblRange[1].RegisterSpace = 0;
				descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				// texture
				descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				descTblRange[2].NumDescriptors = 3;
				descTblRange[2].BaseShaderRegister = 0; // t0, t1, t2
				descTblRange[2].RegisterSpace = 0;
				descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			// MVP matrix
			rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
			rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			// material & texture
			rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam[1].DescriptorTable.NumDescriptorRanges = 2;
			rootParam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
			rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
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

		rootSignatureDesc.NumParameters = 2;
		rootSignatureDesc.pParameters = &rootParam[0];
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
		viewport.MinDepth = 0.0f;
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

HRESULT createDepthBuffer(ID3D12Resource** ppResource, ID3D12DescriptorHeap** ppDescHeap)
{
	{
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = kWindowWidth;
			resourceDesc.Height = kWindowHeight;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		}

		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 1;
			heapProp.VisibleNodeMask = 1;
		}

		D3D12_CLEAR_VALUE clearVal = { };
		{
			clearVal.Format = DXGI_FORMAT_D32_FLOAT;
			clearVal.DepthStencil.Depth = 1.0f;
			clearVal.DepthStencil.Stencil = 0;
		}

		auto ret = getInstanceOfDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearVal,
			IID_PPV_ARGS(ppResource));
		ThrowIfFailed(ret);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = { };
		{
			descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			descHeapDesc.NumDescriptors = 1;
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			descHeapDesc.NodeMask = 0;
		}

		auto ret = getInstanceOfDevice()->CreateDescriptorHeap(
			&descHeapDesc,
			IID_PPV_ARGS(ppDescHeap));
		ThrowIfFailed(ret);
	}


	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = { };
		{
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
			dsvDesc.Texture2D.MipSlice = 0;
		}

		getInstanceOfDevice()->CreateDepthStencilView(
			*ppResource,
			&dsvDesc,
			(*ppDescHeap)->GetCPUDescriptorHandleForHeapStart());
	}

	return S_OK;
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

