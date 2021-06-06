#include "pmd_actor.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <algorithm>
#include <cstdio>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <timeapi.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "init.h"
#include "util.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Winmm.lib")

using namespace Microsoft::WRL;

ComPtr<ID3D12RootSignature> PmdActor::m_rootSignature = nullptr;
ComPtr<ID3D12PipelineState> PmdActor::m_pipelineState = nullptr;
ComPtr<ID3DBlob> PmdActor::m_vsBlob = nullptr;
ComPtr<ID3DBlob> PmdActor::m_psBlob = nullptr;

struct PMDHeader
{
	FLOAT version = 0.0f;
	CHAR model_name[20] = { };
	CHAR comment[256] = { };
};

#pragma pack(1)
struct PMDMaterial
{
	DirectX::XMFLOAT3 diffuse = { };
	FLOAT alpha = 0.0f;
	FLOAT specularity = 0.0f;
	DirectX::XMFLOAT3 specular = { };
	DirectX::XMFLOAT3 ambient = { };
	UINT8 toonIdx = 0;
	UINT8 edgeFlg = 0;
	UINT indicesNum = 0;
	CHAR texFilePath[20] = "";
};
#pragma pack()
static_assert(sizeof(PMDMaterial) == 70);

#pragma pack(1)
struct PMDBone
{
	CHAR boneName[20] = "";
	UINT16 parentNo = 0;
	UINT16 nextNo = 0;
	UCHAR type = 0;
	UINT16 ikBoneNo = 0;
	DirectX::XMFLOAT3 pos = { };
};
#pragma pack()
static_assert(sizeof(PMDBone) == 39);

#pragma pack(1)
struct VMDMotion
{
	char boneName[15] = { };
	uint32_t frameNo = 0;
	DirectX::XMFLOAT3 location = { };
	DirectX::XMFLOAT4 quaternion = { };
	uint8_t bezier[64] = { };
};
#pragma pack()
static_assert(sizeof(VMDMotion) == 111);

static constexpr D3D12_INPUT_ELEMENT_DESC kInputLayout[] = {
	{
		"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"BONENO", 0, DXGI_FORMAT_R16G16_UINT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
	{
		"EDGE_FLG", 0, DXGI_FORMAT_R8_UINT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
};

const std::vector<PMDVertex> s_debugVertices = {
	PMDVertex{
		DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f),
		DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertex{
		DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f),
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertex{
		DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertex{
		DirectX::XMFLOAT3(-0.5f, 0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertex{
		DirectX::XMFLOAT3(0.0f, -0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertex{
		DirectX::XMFLOAT3(0.5f, 0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
};

static constexpr char kSignature[] = "Pmd";
static constexpr size_t kNumSignature = 3;
static constexpr size_t kPmdVertexSize = sizeof(PMDVertex);
const std::vector<UINT16> s_debugIndices = { 0, 1, 2, 3, 4, 5 };

static HRESULT setViewportScissor();
static std::string getModelPath(PmdActor::Model model);
static std::string getMotionPath();
static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath);
template<typename T>
static std::pair<HRESULT, D3D12_VERTEX_BUFFER_VIEW>
createVertexBufferResource(ComPtr<ID3D12Resource>* vertResource, const std::vector<T>& vertices);
static std::pair<HRESULT, D3D12_INDEX_BUFFER_VIEW> createIndexBufferResource(ComPtr<ID3D12Resource>* ibResource, const std::vector<UINT16>& indices);
static HRESULT createBufferResource(ComPtr<ID3D12Resource>* vertResource, size_t width);
static float getYfromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
static void outputDebugMessage(ID3DBlob* errorBlob);

void PmdActor::release()
{
	m_rootSignature.Reset();
	m_pipelineState.Reset();
	m_vsBlob.Reset();
	m_psBlob.Reset();
}

ID3D12PipelineState* PmdActor::getPipelineState()
{
	if (m_pipelineState)
		return m_pipelineState.Get();

	ThrowIfFailed(createPipelineState());
	return m_pipelineState.Get();
}

ID3D12RootSignature* PmdActor::getRootSignature()
{
	if (m_rootSignature)
		return m_rootSignature.Get();

	ThrowIfFailed(createRootSignature(&m_rootSignature));
	return m_rootSignature.Get();
}

D3D12_PRIMITIVE_TOPOLOGY PmdActor::getPrimitiveTopology()
{
	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

PmdActor::PmdActor()
{
	auto ret = createWhiteTexture();
	ThrowIfFailed(ret);

	ret = createBlackTexture();
	ThrowIfFailed(ret);

	ret = createGrayGradiationTexture();
	ThrowIfFailed(ret);

	ret = createDebugResources();
	ThrowIfFailed(ret);
}

HRESULT PmdActor::loadAsset(Model model)
{
	ThrowIfFailed(loadPmd(model));
	ThrowIfFailed(loadVmd());
	return S_OK;
}

void PmdActor::enableAnimation(bool enable)
{
	m_bAnimation = enable;

	if (!enable)
		return;

	m_animationStartTime = timeGetTime();
}

void PmdActor::update(bool animationReversed)
{
	using namespace DirectX;

	static float angle = 0.0f;
	const auto worldMat = DirectX::XMMatrixRotationY(angle);

	*m_worldMatrixPointer = worldMat;

	if (!m_bAnimation)
		return;

	if (!animationReversed)
		angle += 0.02f;
	else
		angle -= 0.02f;

	updateMotion();
}

HRESULT PmdActor::render(ID3D12DescriptorHeap* sceneDescHeap) const
{
	ThrowIfFalse(getPipelineState() != nullptr);
	Resource::instance()->getCommandList()->SetPipelineState(getPipelineState());

	ThrowIfFalse(getRootSignature() != nullptr);
	Resource::instance()->getCommandList()->SetGraphicsRootSignature(getRootSignature());

	setViewportScissor();
	Resource::instance()->getCommandList()->IASetPrimitiveTopology(getPrimitiveTopology());
	Resource::instance()->getCommandList()->IASetVertexBuffers(0, 1, &m_vbView);
	Resource::instance()->getCommandList()->IASetIndexBuffer(&m_ibView);

	// bind to b0: view & proj matrix
	{
		ThrowIfFalse(sceneDescHeap != nullptr);
		Resource::instance()->getCommandList()->SetDescriptorHeaps(1, &sceneDescHeap);
		Resource::instance()->getCommandList()->SetGraphicsRootDescriptorTable(
			0, // b0
			sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to b1: transform matrix
	{
		Resource::instance()->getCommandList()->SetDescriptorHeaps(1, m_transformDescHeap.GetAddressOf());
		Resource::instance()->getCommandList()->SetGraphicsRootDescriptorTable(
			1, // b1
			m_transformDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to b2: material
	// draw call
	{
		Resource::instance()->getCommandList()->SetDescriptorHeaps(1, m_materialDescHeap.GetAddressOf());

		const auto cbvSrvIncSize = Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
		auto materialH = m_materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		UINT indexOffset = 0;

		for (const auto& m : m_materials)
		{
			Resource::instance()->getCommandList()->SetGraphicsRootDescriptorTable(
				2, // b2
				materialH);

			Resource::instance()->getCommandList()->DrawIndexedInstanced(m.indicesNum, 1, indexOffset, 0, 0);

			materialH.ptr += cbvSrvIncSize;
			indexOffset += m.indicesNum;
		}
	}

	return S_OK;
}

HRESULT PmdActor::createResources()
{
	{
		auto [ret, vbView] = createVertexBufferResource(&m_vertResource, m_vertices);
		ThrowIfFailed(ret);
		m_vbView = vbView;
	}

	{
		auto [ret, ibView] = createIndexBufferResource(&m_ibResource, m_indices);
		ThrowIfFailed(ret);
		m_ibView = ibView;
	}

	auto ret = createTransformResource();
	ThrowIfFailed(ret);

	ret = createMaterialResrouces();
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createWhiteTexture()
{
	constexpr uint32_t width = 4;
	constexpr uint32_t height = 4;
	constexpr uint32_t bpp = 4;

	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = width;
			resourceDesc.Height = height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_whiteTextureResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);
	}

	{
		std::vector<uint8_t> data(width * height * bpp);
		std::fill(std::begin(data), std::end(data), 0xff);

		auto ret = m_whiteTextureResource->WriteToSubresource(
			0,
			nullptr,
			data.data(),
			width * bpp,
			static_cast<UINT>(data.size()));
		ThrowIfFailed(ret);
	}

	return S_OK;
}

HRESULT PmdActor::loadShaders()
{
	ThrowIfFalse(m_vsBlob == nullptr);
	ThrowIfFalse(m_psBlob == nullptr);

	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&m_vsBlob,
		errorBlob.ReleaseAndGetAddressOf()
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob.Get());
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
		errorBlob.ReleaseAndGetAddressOf()
	);

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createRootSignature(ComPtr<ID3D12RootSignature>* rootSignature)
{
	ThrowIfFalse(rootSignature != nullptr);
	ThrowIfFalse(rootSignature->Get() == nullptr);

	D3D12_DESCRIPTOR_RANGE descTblRange[4] = { };
	{
		// view & proj matrix
		descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descTblRange[0].NumDescriptors = 1;
		descTblRange[0].BaseShaderRegister = 0; // b0
		descTblRange[0].RegisterSpace = 0;
		descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// transform matrix
		descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descTblRange[1].NumDescriptors = 1;
		descTblRange[1].BaseShaderRegister = 1; // b1
		descTblRange[1].RegisterSpace = 0;
		descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// material
		descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		descTblRange[2].NumDescriptors = 1;
		descTblRange[2].BaseShaderRegister = 2; // b2
		descTblRange[2].RegisterSpace = 0;
		descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		// texture
		descTblRange[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descTblRange[3].NumDescriptors = 4;
		descTblRange[3].BaseShaderRegister = 0; // t0, t1, t2, t3
		descTblRange[3].RegisterSpace = 0;
		descTblRange[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	// create descriptor table to bind resources (e.g. texture, constant buffer, etc.)
	D3D12_ROOT_PARAMETER rootParam[3] = { };
	{
		// view & proj matrix
		rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[0].DescriptorTable.pDescriptorRanges = &descTblRange[0];
		rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		// transform matrix
		rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
		rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		// material & texture
		rootParam[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParam[2].DescriptorTable.NumDescriptorRanges = 2;
		rootParam[2].DescriptorTable.pDescriptorRanges = &descTblRange[2];
		rootParam[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_STATIC_SAMPLER_DESC samplerDesc[2] = { };
	{
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].MipLODBias = 0.0f;
		samplerDesc[0].MaxAnisotropy = 0;
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc[0].MinLOD = 0.0f;
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[0].ShaderRegister = 0;
		samplerDesc[0].RegisterSpace = 0;
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		samplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		samplerDesc[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		samplerDesc[1].MipLODBias = 0.0f;
		samplerDesc[1].MaxAnisotropy = 0;
		samplerDesc[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc[1].MinLOD = 0.0f;
		samplerDesc[1].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[1].ShaderRegister = 1;
		samplerDesc[1].RegisterSpace = 0;
		samplerDesc[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = { };
	{
		rootSignatureDesc.NumParameters = 3;
		rootSignatureDesc.pParameters = &rootParam[0];
		rootSignatureDesc.NumStaticSamplers = 2;
		rootSignatureDesc.pStaticSamplers = &samplerDesc[0];
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.GetAddressOf(),
		errorBlob.GetAddressOf());

	if (FAILED(ret))
	{
		outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);

	ret = Resource::instance()->getDevice()->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignature->ReleaseAndGetAddressOf())
	);
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createPipelineState()
{
	ThrowIfFalse(m_pipelineState == nullptr);

	if (m_rootSignature == nullptr)
	{
		ThrowIfFailed(createRootSignature(&m_rootSignature));
	}

	if (m_vsBlob == nullptr || m_psBlob == nullptr)
	{
		ThrowIfFailed(loadShaders());
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeDesc = { };
	{
		gpipeDesc.pRootSignature = m_rootSignature.Get();
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
		gpipeDesc.InputLayout = { kInputLayout, static_cast<UINT>(_countof(kInputLayout)) };
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

	auto ret = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpipeDesc,
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::loadPmd(Model model)
{
	const std::string modelPath = getModelPath(model);

	FILE* fp = nullptr;
	ThrowIfFalse(fopen_s(&fp, modelPath.c_str(), "rb") == 0);
	{
		char signature[kNumSignature] = { };
		ThrowIfFalse(fread(signature, sizeof(signature), 1, fp) == 1);
		ThrowIfFalse(strncmp(signature, kSignature, kNumSignature) == 0);

		PMDHeader pmdHeader = { };
		ThrowIfFalse(fread(&pmdHeader, sizeof(pmdHeader), 1, fp) == 1);

		ThrowIfFalse(fread(&m_vertNum, sizeof(m_vertNum), 1, fp) == 1);

		m_vertices.resize(m_vertNum * kPmdVertexSize);
		ThrowIfFalse(fread(m_vertices.data(), m_vertices.size(), 1, fp) == 1);

		ThrowIfFalse(fread(&m_indicesNum, sizeof(m_indicesNum), 1, fp) == 1);

		m_indices.resize(m_indicesNum);
		ThrowIfFalse(fread(m_indices.data(), m_indices.size() * sizeof(m_indices[0]), 1, fp) == 1);

		UINT materialNum = 0;
		ThrowIfFalse(fread(&materialNum, sizeof(materialNum), 1, fp) == 1);

		std::vector<PMDMaterial> pmdMaterials(materialNum);
		ThrowIfFalse(fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp) == 1);

		UINT16 boneNum = 0;
		ThrowIfFalse(fread(&boneNum, sizeof(boneNum), 1, fp) == 1);

		std::vector<PMDBone> pmdBones(boneNum);
		ThrowIfFalse(fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp) == boneNum);

		uint16_t ikNum = 0;
		ThrowIfFalse(fread(&ikNum, sizeof(ikNum), 1, fp) == 1);

		m_pmdIks.resize(ikNum);

		for (auto& ik : m_pmdIks)
		{
			ThrowIfFalse(fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp) == 1);
			ThrowIfFalse(fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp) == 1);

			uint8_t chainLen = 0;
			ThrowIfFalse(fread(&chainLen, sizeof(chainLen), 1, fp) == 1);

			ik.nodeIdxes.resize(chainLen);
			ThrowIfFalse(fread(&ik.iterations, sizeof(ik.iterations), 1, fp) == 1);
			ThrowIfFalse(fread(&ik.limit, sizeof(ik.limit), 1, fp) == 1);

			if (chainLen == 0)
				continue;

			ThrowIfFalse(fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]), chainLen, fp) == chainLen);
		}

		// load materials
		{
			m_materials.resize(pmdMaterials.size());

			for (uint32_t i = 0; i < pmdMaterials.size(); ++i)
			{
				m_materials[i].indicesNum = pmdMaterials[i].indicesNum;
				m_materials[i].material.diffuse = pmdMaterials[i].diffuse;
				m_materials[i].material.alpha = pmdMaterials[i].alpha;
				m_materials[i].material.specular = pmdMaterials[i].specular;
				m_materials[i].material.specularity = pmdMaterials[i].specularity;
				m_materials[i].material.ambient = pmdMaterials[i].ambient;
			}

			m_toonResources.resize(pmdMaterials.size());
			m_textureResources.resize(pmdMaterials.size());
			m_sphResources.resize(pmdMaterials.size());
			m_spaResources.resize(pmdMaterials.size());

			for (uint32_t i = 0; i < pmdMaterials.size(); ++i)
			{
				m_toonResources[i] = nullptr;
				m_textureResources[i] = nullptr;
				m_sphResources[i] = nullptr;
				m_spaResources[i] = nullptr;

				{
					std::string toonFilePath = "toon/";
					char toonFileName[16] = "";

					int32_t ret = sprintf_s(
						toonFileName,
						"toon%02d.bmp",
						pmdMaterials[i].toonIdx + 1);
					ThrowIfFalse(ret != -1);

					toonFilePath += toonFileName;

					m_toonResources[i] = loadTextureFromFile(toonFilePath);
				}

				if (strlen(pmdMaterials[i].texFilePath) == 0)
					continue;

				std::string texFileName = pmdMaterials[i].texFilePath;
				std::string sphFileName = std::string();
				std::string spaFileName = std::string();

				if (char splitter = '*'; std::count(texFileName.begin(), texFileName.end(), splitter) > 0)
				{
					const auto namepair = Util::splitFileName(texFileName, splitter);

					if (Util::getExtension(namepair.first) == "sph")
					{
						sphFileName = namepair.first;
					}
					else if (Util::getExtension(namepair.first) == "spa")
					{
						spaFileName = namepair.first;
					}
					else
					{
						texFileName = namepair.first;
					}

					if (Util::getExtension(namepair.second) == "sph")
					{
						sphFileName = namepair.second;
					}
					else if (Util::getExtension(namepair.second) == "spa")
					{
						spaFileName = namepair.second;
					}
					else
					{
						texFileName = namepair.second;
					}
				}
				else
				{
					if (Util::getExtension(texFileName) == "sph")
					{
						sphFileName = texFileName;
						texFileName.clear();
					}
					else if (Util::getExtension(texFileName) == "spa")
					{
						spaFileName = texFileName;
						texFileName.clear();
					}
				}

				if (!texFileName.empty())
				{
					const auto texFilePath = getTexturePathFromModelAndTexPath(modelPath, texFileName.c_str());
					m_textureResources[i] = loadTextureFromFile(texFilePath);
				}

				if (!sphFileName.empty())
				{
					const auto sphFilePath = getTexturePathFromModelAndTexPath(modelPath, sphFileName.c_str());
					m_sphResources[i] = loadTextureFromFile(sphFilePath);
				}

				if (!spaFileName.empty())
				{
					const auto spaFilePath = getTexturePathFromModelAndTexPath(modelPath, spaFileName.c_str());
					m_spaResources[i] = loadTextureFromFile(spaFilePath);
				}
			}
		}

		// load bones
		{
			m_boneNameArray.resize(pmdBones.size());
			m_boneNodeAddressArray.resize(pmdBones.size());

			// create bone node map
			for (uint32_t i = 0; i < pmdBones.size(); ++i)
			{
				const PMDBone& pb = pmdBones.at(i);
				BoneNode& node = m_boneNodeTable[pb.boneName];
				node.boneIdx = i;
				node.startPos = pb.pos;
				node.boneType = pb.type;
				node.ikParentBone = pb.ikBoneNo;

				m_boneNameArray[i] = pb.boneName;
				m_boneNodeAddressArray[i] = &m_boneNodeTable[pb.boneName];
			}

			// construct parent-child map
			for (const PMDBone& bone : pmdBones)
			{
				if (bone.parentNo >= pmdBones.size())
					continue;

				const std::string& parentName = m_boneNameArray.at(bone.parentNo);
				m_boneNodeTable[parentName].children.emplace_back(&m_boneNodeTable[bone.boneName]);
			}
		}

		{
			m_boneMatrices.resize(pmdBones.size());
			std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), DirectX::XMMatrixIdentity());
		}
	}
	ThrowIfFalse(fclose(fp) == 0);

	ThrowIfFailed(createResources());

	DebugOutputFormatString("Vertex num  : %d\n", m_vertNum);
	DebugOutputFormatString("Index num   : %d\n", m_indicesNum);
	DebugOutputFormatString("Material num: %zd\n", m_materials.size());
	DebugOutputFormatString("Bone num    : %zd\n", m_boneMatrices.size());

#define PRINT_DEBUG_IK_DATA (1)
#if PRINT_DEBUG_IK_DATA
	{
		auto getNameFromIdx = [&](uint16_t idx) -> std::string
		{
			auto it = std::find_if(m_boneNodeTable.begin(), m_boneNodeTable.end(),
				[idx](const std::pair<std::string, BoneNode>& obj)
				{
					return obj.second.boneIdx == idx;
				});

			if (it != m_boneNodeTable.end())
				return it->first;

			return std::string("");
		};

		for (const auto& ik : m_pmdIks)
		{
			DebugOutputFormatString("IK bone # = %d : %s\n", ik.boneIdx, getNameFromIdx(ik.boneIdx).c_str());

			for (const auto& node : ik.nodeIdxes)
			{
				DebugOutputFormatString("\tNode bone = %d : %s\n", node, getNameFromIdx(node).c_str());
			}
		}
	}
#endif // PRINT_DEBUG_IK_DATA

	return S_OK;
}

HRESULT PmdActor::loadVmd()
{
	const std::string motionPath = getMotionPath();

	FILE* fp = nullptr;
	ThrowIfFalse(fopen_s(&fp, motionPath.c_str(), "rb") == 0);
	{
		// skip 50 bytes from the beginning
		ThrowIfFalse(fseek(fp, 50, SEEK_SET) == 0);

		uint32_t motionDataNum = 0;
		ThrowIfFalse(fread(&motionDataNum, sizeof(motionDataNum), 1, fp) == 1);

		std::vector<VMDMotion> vmdMotionData(motionDataNum);
		ThrowIfFalse(fread(vmdMotionData.data(), sizeof(VMDMotion), motionDataNum, fp) == motionDataNum);

		for (const VMDMotion& vmdMotion : vmdMotionData)
		{
			m_motionData[vmdMotion.boneName].emplace_back(
				Motion(vmdMotion.frameNo,
					DirectX::XMLoadFloat4(&vmdMotion.quaternion),
					DirectX::XMFLOAT3(), // TODO: needs to be updated
					DirectX::XMFLOAT2(static_cast<float>(vmdMotion.bezier[3]) / 127.0f, static_cast<float>(vmdMotion.bezier[7]) / 127.0f),
					DirectX::XMFLOAT2(static_cast<float>(vmdMotion.bezier[11]) / 127.0f, static_cast<float>(vmdMotion.bezier[15]) / 127.0f)));

			m_duration = std::max<uint32_t>(m_duration, vmdMotion.frameNo);
		}

		for (auto& motionData : m_motionData)
		{
			std::sort(
				motionData.second.begin(),
				motionData.second.end(),
				[](const Motion& lval, const Motion& rval)
				{
					return lval.frameNo <= rval.frameNo;
				});
		}

		DebugOutputFormatString("Motion num  : %d\n", motionDataNum);
		DebugOutputFormatString("Duration    : %d\n", m_duration);
	}
	ThrowIfFalse(fclose(fp) == 0);

	return S_OK;
}

ComPtr<ID3D12Resource> PmdActor::loadTextureFromFile(const std::string& texPath)
{
	const auto it = m_resourceTable.find(texPath);

	if (it != m_resourceTable.end())
		return it->second;

	using namespace DirectX;

	TexMetadata metadata = { };
	ScratchImage scratchImg = { };

	auto ret = LoadFromWICFile(
		Util::getWideStringFromString(texPath).c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg);

	if (FAILED(ret))
		return nullptr;

	const auto img = scratchImg.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = { };
	{
		heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;
	}

	D3D12_RESOURCE_DESC resourceDesc = { };
	{
		resourceDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
		resourceDesc.Alignment = 0;
		resourceDesc.Width = metadata.width;
		resourceDesc.Height = static_cast<UINT>(metadata.height);
		resourceDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
		resourceDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
		resourceDesc.Format = metadata.format;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	ComPtr<ID3D12Resource> resource = nullptr;
	{
		ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);

		ret = resource->WriteToSubresource(
			0,
			nullptr,
			img->pixels,
			static_cast<UINT>(img->rowPitch),
			static_cast<UINT>(img->slicePitch));
		ThrowIfFailed(ret);
	}

	m_resourceTable[texPath] = resource.Get();

	return resource;
}

HRESULT PmdActor::createBlackTexture()
{
	constexpr uint32_t width = 4;
	constexpr uint32_t height = 4;
	constexpr uint32_t bpp = 4;

	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = width;
			resourceDesc.Height = height;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_blackTextureResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);
	}

	{
		std::vector<uint8_t> data(width * height * bpp);
		std::fill(std::begin(data), std::end(data), 0x0);

		auto ret = m_blackTextureResource->WriteToSubresource(
			0,
			nullptr,
			data.data(),
			width * bpp,
			static_cast<UINT>(data.size()));
		ThrowIfFailed(ret);
	}

	return S_OK;
}

HRESULT PmdActor::createGrayGradiationTexture()
{
	constexpr UINT64 width = 4;
	constexpr UINT64 height = 256;
	constexpr UINT64 bpp = 4;

	D3D12_HEAP_PROPERTIES heapProp = { };
	{
		heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
		heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
		heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
		heapProp.CreationNodeMask = 0;
		heapProp.VisibleNodeMask = 0;
	}
	D3D12_RESOURCE_DESC resourceDesc = { };
	{
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.Width = width;
		resourceDesc.Height = height;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.MipLevels = 1;
		resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		resourceDesc.SampleDesc = { 1, 0 };
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_grayGradiationTextureResource.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	std::vector<uint32_t> data(width * height);
	{
		uint32_t c = 0xff;

		for (auto it = data.begin(); it != data.end(); it += width)
		{
			const uint32_t col = (c << 24) | (c << 16) | (c << 8) | c;
			std::fill(it, it + width, col);
			--c;
		}
	}

	ret = m_grayGradiationTextureResource->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		width * bpp,
		static_cast<UINT>(data.size()));
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createDebugResources()
{
	ComPtr<ID3D12Resource> vertResource = nullptr;
	ComPtr<ID3D12Resource> ibResource = nullptr;

	{
		auto [ret, vbView] = createVertexBufferResource(&vertResource, s_debugVertices);
		m_debugVbView = vbView;
	}
	{
		auto [ret, ibView] = createIndexBufferResource(&ibResource, s_debugIndices);
		m_debugIbView = ibView;
	}

	return S_OK;
}

HRESULT PmdActor::createTransformResource()
{
	{
		const size_t w = Util::alignmentedSize(sizeof(DirectX::XMMATRIX) * (1 + m_boneMatrices.size()), 256);

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
			IID_PPV_ARGS(m_transformResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	// map and copy bone matrices
	{
		// [0]: world matrix
		// [1] .. [N]: bone matrices
		DirectX::XMMATRIX* mappedMatrices = nullptr;

		auto result = m_transformResource.Get()->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&mappedMatrices));
		ThrowIfFailed(result);

		std::copy(m_boneMatrices.begin(), m_boneMatrices.end(), mappedMatrices + 1);

		m_worldMatrixPointer = mappedMatrices;
		m_boneMatrixPointer = mappedMatrices + 1;
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_transformDescHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);
	}

	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = { };
		{
			viewDesc.BufferLocation = m_transformResource->GetGPUVirtualAddress();
			viewDesc.SizeInBytes = static_cast<UINT>(m_transformResource->GetDesc().Width);
		}
		auto handle = m_transformDescHeap->GetCPUDescriptorHandleForHeapStart();

		Resource::instance()->getDevice()->CreateConstantBufferView(
			&viewDesc,
			handle);
	}

	return S_OK;
}

HRESULT PmdActor::createMaterialResrouces()
{
	const auto materialBufferSize = Util::alignmentedSize(sizeof(MaterialForHlsl), 256);
	const UINT64 materialNum = m_materials.size();
	ThrowIfFalse(materialNum == m_textureResources.size());
	ThrowIfFalse(materialNum == m_sphResources.size());
	ThrowIfFalse(materialNum == m_spaResources.size());
	ThrowIfFalse(materialNum == m_toonResources.size());
	ThrowIfFalse(m_whiteTextureResource != nullptr);
	ThrowIfFalse(m_blackTextureResource != nullptr);

	// create resource (CBV)
	//ID3D12Resource* materialResource = nullptr;
	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 1;
			heapProp.VisibleNodeMask = 1;
		}
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = materialBufferSize * materialNum;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(m_materialResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);
	}

	// copy
	{
		UINT8* pMapMaterial = nullptr;
		auto ret = m_materialResource->Map(0, nullptr, reinterpret_cast<void**>(&pMapMaterial));
		ThrowIfFailed(ret);

		for (const auto& m : m_materials)
		{
			*reinterpret_cast<MaterialForHlsl*>(pMapMaterial) = m.material;
			pMapMaterial += materialBufferSize;
		}

		m_materialResource->Unmap(0, nullptr);
	}

	// create view (CBV + SRV)
	{
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			heapDesc.NumDescriptors = static_cast<UINT>(materialNum) * 5; // CBV (material) + SRV (tex) + SRV (sph) + SRV (spa) + SRV(toon)
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.NodeMask = 0;
		}

		auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(
			&heapDesc,
			IID_PPV_ARGS(m_materialDescHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(ret);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
		{
			cbvDesc.BufferLocation = m_materialResource->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(materialBufferSize);
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = { };
		{
			srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = 1;
			srvDesc.Texture2D.PlaneSlice = 0;
			srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		}

		auto descHeapH = m_materialDescHeap->GetCPUDescriptorHandleForHeapStart();
		const auto inc = Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		for (uint32_t i = 0; i < materialNum; ++i)
		{
			Resource::instance()->getDevice()->CreateConstantBufferView(&cbvDesc, descHeapH);

			cbvDesc.BufferLocation += materialBufferSize; // pointing to GPU virtual address
			descHeapH.ptr += inc;

			if (m_textureResources[i] == nullptr)
			{
				srvDesc.Format = m_whiteTextureResource->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_whiteTextureResource.Get(), &srvDesc, descHeapH);
			}
			else
			{
				srvDesc.Format = m_textureResources[i]->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_textureResources[i].Get(), &srvDesc, descHeapH);
			}

			descHeapH.ptr += inc;

			if (m_sphResources[i] == nullptr)
			{
				srvDesc.Format = m_whiteTextureResource->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_whiteTextureResource.Get(), &srvDesc, descHeapH);
			}
			else
			{
				srvDesc.Format = m_sphResources[i]->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_sphResources[i].Get(), &srvDesc, descHeapH);
			}

			descHeapH.ptr += inc;

			if (m_spaResources[i] == nullptr)
			{
				srvDesc.Format = m_blackTextureResource->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_blackTextureResource.Get(), &srvDesc, descHeapH);
			}
			else
			{
				srvDesc.Format = m_spaResources[i]->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_spaResources[i].Get(), &srvDesc, descHeapH);
			}

			descHeapH.ptr += inc;

			if (m_toonResources[i] == nullptr)
			{
				srvDesc.Format = m_grayGradiationTextureResource->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_grayGradiationTextureResource.Get(), &srvDesc, descHeapH);
			}
			else
			{
				srvDesc.Format = m_toonResources[i]->GetDesc().Format;
				Resource::instance()->getDevice()->CreateShaderResourceView(m_toonResources[i].Get(), &srvDesc, descHeapH);
			}

			descHeapH.ptr += inc;
		}
	}

	return S_OK;
}

void PmdActor::updateMotion()
{
	using namespace DirectX;

	constexpr uint32_t kFps = 30;
	const DWORD elapsedTime = timeGetTime() - m_animationStartTime;
	uint32_t frameNo = static_cast<uint32_t>(kFps * (elapsedTime / 1000.0f));

	if (frameNo > m_duration)
	{
		m_animationStartTime = timeGetTime();
		frameNo = 0;
	}

	// clear bone matrices with identity
	std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), DirectX::XMMatrixIdentity());

#define TEST0 (0)
#if TEST0
	{
		const auto armNode = m_boneNodeTable["左腕"];
		const XMMATRIX armMat = XMMatrixTranslation(-armNode.startPos.x, -armNode.startPos.y, -armNode.startPos.z)
			* XMMatrixRotationZ(XM_PIDIV2)
			* XMMatrixTranslation(armNode.startPos.x, armNode.startPos.y, armNode.startPos.z);

		const auto elbowNode = m_boneNodeTable["左ひじ"];
		const XMMATRIX elbowMat = XMMatrixTranslation(-elbowNode.startPos.x, -elbowNode.startPos.y, -elbowNode.startPos.z)
			* XMMatrixRotationZ(-XM_PIDIV2)
			* XMMatrixTranslation(elbowNode.startPos.x, elbowNode.startPos.y, elbowNode.startPos.z);

		m_boneMatrices[armNode.boneIdx] = armMat;
		m_boneMatrices[elbowNode.boneIdx] = elbowMat;
	}
#endif // TEST0

#define TEST1 (0)
#if TEST1
	{
		for (const auto& boneMotion : m_motionData)
		{
			const BoneNode node = m_boneNodeTable[boneMotion.first];
			const XMFLOAT3& pos = node.startPos;
			const XMMATRIX mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
				* XMMatrixRotationQuaternion(boneMotion.second[0].quaternion)
				* XMMatrixTranslation(pos.x, pos.y, pos.z);
			m_boneMatrices[node.boneIdx] = mat;
		}
	}
#endif // TEST1

	for (const std::pair<std::string, std::vector<Motion>>& boneMotion : m_motionData)
	{
		const auto itBoneNode = m_boneNodeTable.find(boneMotion.first);

		if (itBoneNode == m_boneNodeTable.end())
			continue;

		const std::vector<Motion> motions = boneMotion.second;

		auto rit = std::find_if(
			motions.rbegin(),
			motions.rend(),
			[frameNo](const Motion& motion)
			{
				return motion.frameNo <= frameNo;
			});

		if (rit == motions.rend())
			continue;

		XMMATRIX rotation;
		auto it = rit.base();

		if (it != motions.end())
		{
			// interpolation with Bezier curve
			float t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);
			t = getYfromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		const BoneNode node = m_boneNodeTable[boneMotion.first];
		const XMMATRIX mat = DirectX::XMMatrixTranslation(-node.startPos.x, -node.startPos.y, -node.startPos.z)
			* rotation
			* DirectX::XMMatrixTranslation(node.startPos.x, node.startPos.y, node.startPos.z);

		m_boneMatrices[node.boneIdx] = mat;
	}

	recursiveMatrixMultiply(m_boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	std::copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_boneMatrixPointer);
}

void PmdActor::recursiveMatrixMultiply(const BoneNode& node, const DirectX::XMMATRIX& mat)
{
	m_boneMatrices[node.boneIdx] *= mat;

	for (const auto& cnode : node.children)
	{
		recursiveMatrixMultiply(*cnode, m_boneMatrices[node.boneIdx]);
	}
}

static HRESULT setViewportScissor()
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

	Resource::instance()->getCommandList()->RSSetViewports(1, &viewport);

	D3D12_RECT scissorRect = { };
	{
		scissorRect.top = 0;
		scissorRect.left = 0;
		scissorRect.right = scissorRect.left + kWindowWidth;
		scissorRect.bottom = scissorRect.top + kWindowHeight;
	}

	Resource::instance()->getCommandList()->RSSetScissorRects(1, &scissorRect);

	return S_OK;
}

static std::string getModelPath(PmdActor::Model model)
{
	switch (model) {
	case PmdActor::Model::kMiku: return "Model/初音ミク.pmd";
	case PmdActor::Model::kMikuMetal: return "Model/初音ミクmetal.pmd";
	case PmdActor::Model::kLuka: return "Model/巡音ルカ.pmd";
	case PmdActor::Model::kLen: return "Model/鏡音レン.pmd";
	case PmdActor::Model::kKaito: return "Model/カイト.pmd";
	case PmdActor::Model::kHaku: return "Model/弱音ハク.pmd";
	case PmdActor::Model::kRin: return "Model/鏡音リン.pmd";
	case PmdActor::Model::kMeiko: return "Model/咲音メイコ.pmd";
	case PmdActor::Model::kNeru: return "Model/亞北ネル.pmd";
	default: ThrowIfFalse(false); break;
	}

	return "";
}

std::string getMotionPath()
{
	return "Motion/motion.vmd";
}

static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
{
#if 0
	const auto folderPath = modelPath.substr(0, modelPath.rfind('/'));
#else
	const int32_t pathIndex1 = static_cast<int32_t>(modelPath.rfind('/'));
	const int32_t pathIndex2 = static_cast<int32_t>(modelPath.rfind('\\'));
	const int32_t pathIndex = (std::max)(pathIndex1, pathIndex2) + 1;
	const auto folderPath = modelPath.substr(0, pathIndex);
#endif
	return folderPath + texPath;
}

template<typename T>
static std::pair<HRESULT, D3D12_VERTEX_BUFFER_VIEW>
createVertexBufferResource(ComPtr<ID3D12Resource>* vertResource, const std::vector<T>& vertices)
{
	ThrowIfFalse(vertResource != nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	{
		const size_t sizeInBytes = vertices.size() * sizeof(vertices[0]);

		ThrowIfFailed(createBufferResource(vertResource, sizeInBytes));
		ThrowIfFalse((*vertResource) != nullptr);
		{
			vbView.BufferLocation = (*vertResource)->GetGPUVirtualAddress();
			vbView.SizeInBytes = static_cast<UINT>(sizeInBytes);
			vbView.StrideInBytes = kPmdVertexSize;
		}

		T* vertMap = nullptr;
		auto ret = (*vertResource)->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&vertMap)
		);
		ThrowIfFailed(ret);

		std::copy(std::begin(vertices), std::end(vertices), vertMap);

		(*vertResource)->Unmap(0, nullptr);
	}

	return { S_OK, vbView };
}

static std::pair<HRESULT, D3D12_INDEX_BUFFER_VIEW> createIndexBufferResource(ComPtr<ID3D12Resource>* ibResource, const std::vector<UINT16>& indices)
{
	ThrowIfFalse(ibResource != nullptr);

	D3D12_INDEX_BUFFER_VIEW ibView = { };
	{
		const size_t sizeInBytes = indices.size() * sizeof(indices[0]);

		ThrowIfFailed(createBufferResource(ibResource, sizeInBytes));
		ThrowIfFalse((*ibResource) != nullptr);
		{
			ibView.BufferLocation = (*ibResource)->GetGPUVirtualAddress();
			ibView.SizeInBytes = static_cast<UINT>(sizeInBytes);
			ibView.Format = DXGI_FORMAT_R16_UINT;
		}

		UINT16* ibMap = nullptr;
		auto ret = (*ibResource)->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&ibMap));
		ThrowIfFailed(ret);

		std::copy(std::begin(indices), std::end(indices), ibMap);

		(*ibResource)->Unmap(0, nullptr);
	}

	return { S_OK, ibView };
}

static HRESULT createBufferResource(ComPtr<ID3D12Resource>* resource, size_t width)
{
	{
		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 1;
			heapProp.VisibleNodeMask = 1;
		}
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = width;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(resource->ReleaseAndGetAddressOf())
		);
		ThrowIfFailed(ret);
	}

	return S_OK;
}

static float getYfromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
		return x;

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x; // coefficient of t^3
	const float k1 = 3 * b.x - 6 * a.x; // coefficient of t^2
	const float k2 = 3 * a.x; // coefficient of t

	constexpr float epsilon = 0.0005f;

	for (int32_t i = 0; i < n; ++i)
	{
		const float ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		if (-epsilon <= ft && ft <= epsilon)
			break;

		t -= ft / 2;
	}

	const float r = 1 - t;

	return (t * t * t) + (3 * t * t * r * b.y) + (3 * t * r * r * a.y);
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
