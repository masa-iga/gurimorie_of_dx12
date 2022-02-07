#include "floor.h"
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

constexpr float kScalingFactor = 100.0f;

struct Vertex
{
	DirectX::XMFLOAT3 pos = { };
};

constexpr Vertex kFloorVertices[] =
{
	DirectX::XMFLOAT3(-1.0f, 0.0f, -1.0f),
	DirectX::XMFLOAT3(-1.0f, 0.0f,  1.0f),
	DirectX::XMFLOAT3( 1.0f, 0.0f,  1.0f),
	DirectX::XMFLOAT3(-1.0f, 0.0f, -1.0f),
	DirectX::XMFLOAT3( 1.0f, 0.0f,  1.0f),
	DirectX::XMFLOAT3( 1.0f, 0.0f, -1.0f),
};

constexpr Vertex kAxisVertices[] =
{
	DirectX::XMFLOAT3(-1.0f,  0.0f,  0.0f), // X axis
	DirectX::XMFLOAT3( 1.0f,  0.0f,  0.0f),
	DirectX::XMFLOAT3( 0.0f,  0.0f, -1.0f), // Z axis
	DirectX::XMFLOAT3( 0.0f,  0.0f,  1.0f),
	DirectX::XMFLOAT3( 0.95f, 0.0f,  0.05f), // Arrow on X axis
	DirectX::XMFLOAT3( 1.0f,  0.0f,  0.0f),
	DirectX::XMFLOAT3( 0.95f, 0.0f, -0.05f),
	DirectX::XMFLOAT3( 1.0f,  0.0f,  0.0f),
	DirectX::XMFLOAT3( 0.0f, -0.05f, 0.95f), // Arror on Z axis
	DirectX::XMFLOAT3( 0.0f,  0.0f,  1.0f),
	DirectX::XMFLOAT3( 0.0f,  0.05f, 0.95f),
	DirectX::XMFLOAT3( 0.0f,  0.0f,  1.0f),
	DirectX::XMFLOAT3(-1.0f / kScalingFactor,  0.0f, -1.0f / kScalingFactor), // Unit line on X axis
	DirectX::XMFLOAT3(-1.0f / kScalingFactor,  0.0f,  1.0f / kScalingFactor),
	DirectX::XMFLOAT3( 1.0f / kScalingFactor,  0.0f, -1.0f / kScalingFactor),
	DirectX::XMFLOAT3( 1.0f / kScalingFactor,  0.0f,  1.0f / kScalingFactor),
	DirectX::XMFLOAT3(-1.0f / kScalingFactor,  0.0f, -1.0f / kScalingFactor), // Unit line on Z axis
	DirectX::XMFLOAT3( 1.0f / kScalingFactor,  0.0f, -1.0f / kScalingFactor),
	DirectX::XMFLOAT3(-1.0f / kScalingFactor,  0.0f,  1.0f / kScalingFactor),
	DirectX::XMFLOAT3( 1.0f / kScalingFactor,  0.0f,  1.0f / kScalingFactor),
};

HRESULT Floor::init()
{
	ThrowIfFailed(loadShaders());
	ThrowIfFailed(createVertexResource());
	ThrowIfFailed(createTransformResource());
	ThrowIfFailed(createRootSignature());
	ThrowIfFailed(createGraphicsPipeline());
	initMatrix();
	return S_OK;
}

HRESULT Floor::renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);
	ThrowIfFalse(depthHeap != nullptr);

	setInputAssembler(list);
	setRasterizer(list, Config::kShadowBufferWidth, Config::kShadowBufferHeight);

	{
		auto depthHandle = depthHeap->GetCPUDescriptorHandleForHeapStart();
		list->OMSetRenderTargets(0, nullptr, false, &depthHandle);
	}

	list->SetPipelineState(m_pipelineStates.at(PipelineType::kShadow).Get());
	list->SetGraphicsRootSignature(m_rootSignature.Get());

	list->SetDescriptorHeaps(1, &sceneDescHeap);
	list->SetGraphicsRootDescriptorTable(0 /* root param 0 */, sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	list->SetDescriptorHeaps(1, m_transDescHeap.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(1 /* root param 1 */, m_transDescHeap.Get()->GetGPUDescriptorHandleForHeapStart());

	list->DrawInstanced(_countof(kFloorVertices), 1, 0, 0);

	return S_OK;
}

HRESULT Floor::render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthLightSrvHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);

	setInputAssembler(list);
	setRasterizer(list, Config::kWindowWidth, Config::kWindowHeight);

	list->SetPipelineState(m_pipelineStates.at(PipelineType::kMesh).Get());
	list->SetGraphicsRootSignature(m_rootSignature.Get());

	list->SetDescriptorHeaps(1, &sceneDescHeap);
	list->SetGraphicsRootDescriptorTable(0 /* root param 0 */, sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	list->SetDescriptorHeaps(1, m_transDescHeap.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(1 /* root param 1 */, m_transDescHeap.Get()->GetGPUDescriptorHandleForHeapStart());

	list->SetDescriptorHeaps(1, &depthLightSrvHeap);
	list->SetGraphicsRootDescriptorTable(2 /* root param 2 */, depthLightSrvHeap->GetGPUDescriptorHandleForHeapStart());

	list->DrawInstanced(_countof(kFloorVertices), 1, 0, 0);

	return S_OK;
}

HRESULT Floor::renderAxis(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap)
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);

	{
		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		list->IASetVertexBuffers(0, 1, &m_vbViews.at(VbType::kAxis));
	}
	setRasterizer(list, Config::kWindowWidth, Config::kWindowHeight);

	list->SetPipelineState(m_pipelineStates.at(PipelineType::kAxis).Get());
	list->SetGraphicsRootSignature(m_rootSignature.Get());

	list->SetDescriptorHeaps(1, &sceneDescHeap);
	list->SetGraphicsRootDescriptorTable(0 /* root param 0 */, sceneDescHeap->GetGPUDescriptorHandleForHeapStart());

	list->SetDescriptorHeaps(1, m_transDescHeap.GetAddressOf());
	list->SetGraphicsRootDescriptorTable(1 /* root param 1 */, m_transDescHeap.Get()->GetGPUDescriptorHandleForHeapStart());

	list->DrawInstanced(_countof(kAxisVertices), 1, 0, 0);

	return S_OK;
}

void Floor::initMatrix()
{
	{
		ThrowIfFalse(m_transResource != nullptr);
		auto result = m_transResource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&m_pTransMatrix));
		ThrowIfFailed(result);
	}

	m_pTransMatrix->meshMatrix = kDefaultTransMat * kDefaultScaleMat;
	m_pTransMatrix->axisMatrix = DirectX::XMMatrixScaling(kScalingFactor, kScalingFactor, kScalingFactor);

	{
		m_transResource.Get()->Unmap(0, nullptr);
	}
}

HRESULT Floor::loadShaders()
{
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kVsEntryPoints.at(VsType::kBasic),
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsArray.at(VsType::kBasic).ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kVsEntryPoints.at(VsType::kShadow),
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsArray.at(VsType::kShadow).ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kVsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kVsEntryPoints.at(VsType::kAxis),
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsArray.at(VsType::kAxis).ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kPsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kPsEntryPoints.at(PsType::kBasic),
		Constant::kPsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_psArray.at(PsType::kBasic).ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = D3DCompileFromFile(
		kPsFile,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		kPsEntryPoints.at(PsType::kAxis),
		Constant::kPsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_psArray.at(PsType::kAxis).ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	return S_OK;
}

HRESULT Floor::createVertexResource()
{
	{
		constexpr size_t bufferSize = sizeof(kFloorVertices);
		const auto w = static_cast<uint32_t>(Util::alignmentedSize(bufferSize, Constant::kD3D12ConstantBufferAlignment));

		auto& resource = m_vertResources.at(VbType::kMesh);

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
				resourceDesc.SampleDesc = { 1 , 0 };
				resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			}

			auto result = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = resource.Get()->SetName(Util::getWideStringFromString("FloorMeshVertexBuffer").c_str());
			ThrowIfFailed(result);
		}

		{
			auto& vbView = m_vbViews.at(VbType::kMesh);
			vbView.BufferLocation = resource.Get()->GetGPUVirtualAddress();
			vbView.SizeInBytes = w;
			vbView.StrideInBytes = sizeof(Vertex);
		}

		{
			Vertex* pVertices = nullptr;

			auto result = resource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
			ThrowIfFailed(result);

			std::copy(std::begin(kFloorVertices), std::end(kFloorVertices), pVertices);

			resource.Get()->Unmap(0, nullptr);
		}
	}

	{
		constexpr size_t bufferSize = sizeof(kAxisVertices);
		const auto w = static_cast<uint32_t>(Util::alignmentedSize(bufferSize, Constant::kD3D12ConstantBufferAlignment));

		D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer({ w, 0 });

		auto& resource = m_vertResources.at(VbType::kAxis);

		{
			auto result = Resource::instance()->getDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
			ThrowIfFailed(result);

			result = resource.Get()->SetName(Util::getWideStringFromString("FloorAxisVertexBuffer").c_str());
			ThrowIfFailed(result);
		}

		{
			auto& vbView = m_vbViews.at(VbType::kAxis);
			vbView.BufferLocation = resource.Get()->GetGPUVirtualAddress();
			vbView.SizeInBytes = w;
			vbView.StrideInBytes = sizeof(Vertex);
		}

		{
			Vertex* pVertices = nullptr;

			auto result = resource.Get()->Map(0, nullptr, reinterpret_cast<void**>(&pVertices));
			ThrowIfFailed(result);

			std::copy(std::begin(kAxisVertices), std::end(kAxisVertices), pVertices);

			resource.Get()->Unmap(0, nullptr);
		}
	}

	return S_OK;
}

HRESULT Floor::createTransformResource()
{
	const auto bufferSize = Util::alignmentedSize(sizeof(*m_pTransMatrix), Constant::kD3D12ConstantBufferAlignment);

	{
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_transResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_transResource.Get()->SetName(Util::getWideStringFromString("FloorTransBuffer").c_str());
		ThrowIfFailed(result);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 1;
		}

		auto result = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_transDescHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_transDescHeap.Get()->SetName(Util::getWideStringFromString("FloorTransDescHeap").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {
			m_transResource.Get()->GetGPUVirtualAddress(),
			static_cast<UINT>(m_transResource.Get()->GetDesc().Width) };
		const auto handle = m_transDescHeap.Get()->GetCPUDescriptorHandleForHeapStart();

		Resource::instance()->getDevice()->CreateConstantBufferView(&viewDesc, handle);
	}

	return S_OK;
}

HRESULT Floor::createRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descRange[3] = { };
	{
		descRange[0] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0); // b0: scene matrix
		descRange[1] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1); // b1: world matrix
		descRange[2] = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0: depth light map
	}

	D3D12_ROOT_PARAMETER rootParam[3] = { };
	{
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[0].DescriptorTable.pDescriptorRanges = &descRange[0];
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[1].DescriptorTable.pDescriptorRanges = &descRange[1];
		rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[2].DescriptorTable.pDescriptorRanges = &descRange[2];
		rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_STATIC_SAMPLER_DESC samplerDesc = { };
	{
		samplerDesc = CD3DX12_STATIC_SAMPLER_DESC(0);
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.MaxAnisotropy = 1;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	{
		rootSignatureDesc.NumParameters = 3;
		rootSignatureDesc.pParameters = &rootParam[0];
		rootSignatureDesc.NumStaticSamplers = 1;
		rootSignatureDesc.pStaticSamplers = &samplerDesc;
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.GetAddressOf(),
		errorBlob.GetAddressOf());

	if (FAILED(result))
	{
		Debug::outputDebugMessage(errorBlob.Get());
		return E_FAIL;
	}

	result = Resource::instance()->getDevice()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf()));
	ThrowIfFailed(result);

	result = m_rootSignature.Get()->SetName(Util::getWideStringFromString("FloorRootSignature").c_str());
	ThrowIfFailed(result);

	return S_OK;
}

HRESULT Floor::createGraphicsPipeline()
{
	constexpr D3D12_INPUT_ELEMENT_DESC kInputElementDesc[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = { };
	{
		ThrowIfFalse(m_rootSignature != nullptr);
		pipelineStateDesc.pRootSignature = m_rootSignature.Get();
		pipelineStateDesc.VS = { m_vsArray.at(VsType::kBasic).Get()->GetBufferPointer(), m_vsArray.at(VsType::kBasic).Get()->GetBufferSize() };
		pipelineStateDesc.PS = { m_psArray.at(PsType::kBasic).Get()->GetBufferPointer(), m_psArray.at(PsType::kBasic).Get()->GetBufferSize() };
		//D3D12_SHADER_BYTECODE DS;
		//D3D12_SHADER_BYTECODE HS;
		//D3D12_SHADER_BYTECODE GS;
		//D3D12_STREAM_OUTPUT_DESC StreamOutput;
		pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		{
			pipelineStateDesc.RasterizerState.FrontCounterClockwise = true;
			pipelineStateDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			pipelineStateDesc.RasterizerState.DepthClipEnable = false;
		}
		pipelineStateDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		pipelineStateDesc.InputLayout = { kInputElementDesc, _countof(kInputElementDesc) };
		//D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
		pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		pipelineStateDesc.NumRenderTargets = 2;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineStateDesc.SampleDesc = { 1, 0 };
		//UINT NodeMask;
		//D3D12_CACHED_PIPELINE_STATE CachedPSO;
		//D3D12_PIPELINE_STATE_FLAGS Flags;
	}

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(m_pipelineStates.at(PipelineType::kMesh).ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineStates.at(PipelineType::kMesh).Get()->SetName(Util::getWideStringFromString("FloorGraphicsPipeline").c_str());
		ThrowIfFailed(result);
	}

	// for shadow
	{
		pipelineStateDesc.VS = { m_vsArray.at(VsType::kShadow).Get()->GetBufferPointer(), m_vsArray.at(VsType::kShadow).Get()->GetBufferSize() };
		pipelineStateDesc.PS = { nullptr, 0 };
		pipelineStateDesc.NumRenderTargets = 0;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		pipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	}

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(m_pipelineStates.at(PipelineType::kShadow).ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineStates.at(PipelineType::kShadow)->SetName(Util::getWideStringFromString("FloorGraphicsShadowPipeline").c_str());
		ThrowIfFailed(result);
	}

	// for axis
	{
		pipelineStateDesc.VS = { m_vsArray.at(VsType::kAxis).Get()->GetBufferPointer(), m_vsArray.at(VsType::kAxis).Get()->GetBufferSize() };
		pipelineStateDesc.PS = { m_psArray.at(PsType::kAxis).Get()->GetBufferPointer(), m_psArray.at(PsType::kAxis).Get()->GetBufferSize() };
		pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		pipelineStateDesc.NumRenderTargets = 2;
		pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	{
		auto result = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
			&pipelineStateDesc,
			IID_PPV_ARGS(m_pipelineStates.at(PipelineType::kAxis).ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_pipelineStates.at(PipelineType::kAxis)->SetName(Util::getWideStringFromString("FloorGraphicsAxisPipeline").c_str());
		ThrowIfFailed(result);
	}

	return S_OK;
}

void Floor::setInputAssembler(ID3D12GraphicsCommandList* list) const
{
	ThrowIfFalse(list != nullptr);

	list->IASetPrimitiveTopology(kPrimTopology);
	list->IASetVertexBuffers(0, 1, &m_vbViews.at(VbType::kMesh));
}

void Floor::setRasterizer(ID3D12GraphicsCommandList* list, int32_t width, int32_t height) const
{
	const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	list->RSSetViewports(1, &viewport);

	const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, width, height);
	list->RSSetScissorRects(1, &scissorRect);
}
