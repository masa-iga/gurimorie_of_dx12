#include "render.h"
#include <cassert>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <synchapi.h>
#include "init.h"
#include "debug.h"

#pragma comment(lib, "d3dcompiler.lib")

static HRESULT createRootSignature(ID3D12RootSignature** ppRootSignature);
static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence);
static HRESULT createVertexBuffer();
static void outputDebugMessage(ID3DBlob* errorBlob);

HRESULT Render::init()
{
	ThrowIfFailed(createFence(m_fenceVal, &m_pFence));

	ThrowIfFailed(createVertexBuffer());

	ThrowIfFailed(loadShaders());

	return S_OK;
}

HRESULT Render::render()
{
	ThrowIfFailed(setPipelineState());

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
	constexpr float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	getInstanceOfCommandList()->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


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

HRESULT Render::setPipelineState()
{
	D3D12_INPUT_ELEMENT_DESC elementDescs[] = {
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0,
		},
	};

	ID3D12RootSignature* pRootSignature = nullptr;
	ThrowIfFailed(createRootSignature(&pRootSignature));
	ThrowIfFalse(pRootSignature != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeDesc = { };
	{
		gpipeDesc.pRootSignature = pRootSignature;
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
		gpipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		// DXGI_FORMAT DSVFormat;
		gpipeDesc.SampleDesc = {
			1 /* count */,
			0 /* quality */
		};
		// UINT NodeMask;
		// D3D12_CACHED_PIPELINE_STATE CachedPSO;
		// D3D12_PIPELINE_STATE_FLAGS Flags;
	}

	ID3D12PipelineState* pipelineState = nullptr;
	auto ret = getInstanceOfDevice()->CreateGraphicsPipelineState(&gpipeDesc, IID_PPV_ARGS(&pipelineState));
	ThrowIfFailed(ret);

	return S_OK;
}

static HRESULT createRootSignature(ID3D12RootSignature** ppRootSignature)
{
	ThrowIfFalse(ppRootSignature != nullptr);

	// need to input vertex
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

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

static HRESULT createFence(UINT64 initVal, ID3D12Fence** ppFence)
{
	return getInstanceOfDevice()->CreateFence(
		initVal,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(ppFence));
}

static HRESULT createVertexBuffer()
{
	const DirectX::XMFLOAT3 vertices[] = {
		{-1.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f},
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
	DirectX::XMFLOAT3* vertMap = nullptr;

	ret = vertBuff->Map(
		0,
		nullptr,
		(void**)&vertMap);
	ThrowIfFailed(ret);

	std::copy(std::begin(vertices), std::end(vertices), vertMap);

	vertBuff->Unmap(0, nullptr);


	// create VB view
	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	{
		vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(vertices);
		vbView.StrideInBytes = sizeof(vertices[0]);
	}


	// TODO: getInstanceOfCommandList()->IASetVertexBuffers(0, 1, &vbView);

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

