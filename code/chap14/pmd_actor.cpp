#include "pmd_actor.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <algorithm>
#include <array>
#include <cstdio>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXTex.h>
#include <timeapi.h>
#pragma warning(pop)
#include "config.h"
#include "constant.h"
#include "debug.h"
#include "init.h"
#include "loader.h"
#include "util.h"

#undef min
#undef max

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "Winmm.lib")

using namespace Microsoft::WRL;

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

#pragma pack(1)
struct VMDMorph
{
	char name[15] = { };
	uint32_t frameNo = 0;
	float weight = 0.0f;
};
#pragma pack()
static_assert(sizeof(VMDMorph) == 23);

#pragma pack(1)
struct VMDCamera
{
	uint32_t frameNo = 0;
	float distance = 0.0f;
	DirectX::XMFLOAT3 pos = { };
	DirectX::XMFLOAT3 eulerAngle = { };
	uint8_t interpolation[24] = { };
	uint32_t fov = 0;
	uint8_t persFlg = 0;
};
#pragma pack()
static_assert(sizeof(VMDCamera) == 61);

struct VMDLight
{
	DirectX::XMFLOAT3 rgb = { };
	DirectX::XMFLOAT3 vec = { };
};

#pragma pack(1)
struct VMDSelfShadow
{
	uint32_t frameNo = 0;
	uint8_t mode = 0;
	float distance = 0.0f;
};
#pragma pack()
static_assert(sizeof(VMDSelfShadow) == 9);

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
	{
		"PADDING", 0, DXGI_FORMAT_R8G8_UINT, 0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
	},
};

const std::vector<PMDVertexForLoader> s_debugVertices = {
	PMDVertexForLoader{
		DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f),
		DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertexForLoader{
		DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f),
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertexForLoader{
		DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertexForLoader{
		DirectX::XMFLOAT3(-0.5f, 0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertexForLoader{
		DirectX::XMFLOAT3(0.0f, -0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
	PMDVertexForLoader{
		DirectX::XMFLOAT3(0.5f, 0.5f, 0.2f),
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT2(0.0f, 0.0f),
		{ 0, 0 },
		0,
		0
	},
};

static const std::string kModelDir = "../resource/Model";
static const std::string kMotionDir = "../resource/Motion";
static const std::string kToonDir = "../resource/toon";
static constexpr char kSignature[] = "Pmd";
static constexpr size_t kNumSignature = 3;
const std::vector<UINT16> s_debugIndices = { 0, 1, 2, 3, 4, 5 };

static HRESULT setViewportScissor(int32_t width, int32_t height);
static std::string getModelPath(PmdActor::Model model);
static std::string getMotionPath();
static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath);
template<typename T>
static std::pair<HRESULT, D3D12_VERTEX_BUFFER_VIEW>
createVertexBufferResource(ComPtr<ID3D12Resource>* vertResource, const std::vector<T>& vertices);
static std::pair<HRESULT, D3D12_INDEX_BUFFER_VIEW> createIndexBufferResource(ComPtr<ID3D12Resource>* ibResource, const std::vector<UINT16>& indices);
static HRESULT createBufferResource(ComPtr<ID3D12Resource>* vertResource, size_t width);
static float getYfromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);
static DirectX::XMMATRIX lookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right);
static DirectX::XMMATRIX lookAtMatrix(const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right);

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

	ret = createPipelineState();
	ThrowIfFailed(ret);

	ret = createRootSignature(&m_rootSignature);
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
	static DWORD offset = 0;
	m_bAnimation = enable;

	if (!enable)
	{
		offset = timeGetTime() - m_animationStartTime;
		return;
	}

	m_animationStartTime = timeGetTime() - offset;
}

void PmdActor::update(bool animationReversed)
{
	using namespace DirectX;

	static float angle = 0.0f;
	const auto worldMat = DirectX::XMMatrixRotationY(angle);

	*m_worldMatrixPointer = worldMat;

	updateMotion();
}

HRESULT PmdActor::renderShadow(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthHeap) const
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);
	ThrowIfFalse(depthHeap != nullptr);

	ThrowIfFailed(setCommonPipelineConfig(list));

	setViewportScissor(Config::kShadowBufferWidth, Config::kShadowBufferHeight);

	ThrowIfFalse(m_shadowPipelineState != nullptr);
	list->SetPipelineState(m_shadowPipelineState.Get());

	ThrowIfFalse(getRootSignature() != nullptr);
	list->SetGraphicsRootSignature(getRootSignature());

	// this is shadow map path. Unbind render target
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE handle = depthHeap->GetCPUDescriptorHandleForHeapStart();

		list->OMSetRenderTargets(
			0,
			nullptr,
			false,
			&handle);
	}

	// bind to b0: view & proj matrix
	{
		ThrowIfFalse(sceneDescHeap != nullptr);
		list->SetDescriptorHeaps(1, &sceneDescHeap);
		list->SetGraphicsRootDescriptorTable(
			0, // b0
			sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to b1: transform matrix
	{
		list->SetDescriptorHeaps(1, m_transformDescHeap.GetAddressOf());
		list->SetGraphicsRootDescriptorTable(
			1, // b1
			m_transformDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to b2: material
	{
		list->SetDescriptorHeaps(1, m_materialDescHeap.GetAddressOf());
	}

	// draw call
	list->DrawIndexedInstanced(m_indicesNum, 1, 0, 0, 0);

	return S_OK;
}

HRESULT PmdActor::render(ID3D12GraphicsCommandList* list, ID3D12DescriptorHeap* sceneDescHeap, ID3D12DescriptorHeap* depthLightSrvHeap) const
{
	ThrowIfFalse(list != nullptr);
	ThrowIfFalse(sceneDescHeap != nullptr);

	ThrowIfFailed(setCommonPipelineConfig(list));

	ThrowIfFalse(getPipelineState() != nullptr);
	list->SetPipelineState(getPipelineState());

	ThrowIfFalse(getRootSignature() != nullptr);
	list->SetGraphicsRootSignature(getRootSignature());

	// bind to root param 0: view & proj matrix
	{
		ThrowIfFalse(sceneDescHeap != nullptr);
		list->SetDescriptorHeaps(1, &sceneDescHeap);
		list->SetGraphicsRootDescriptorTable(
			0, // root param 0
			sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to root param 1: transform matrix
	{
		list->SetDescriptorHeaps(1, m_transformDescHeap.GetAddressOf());
		list->SetGraphicsRootDescriptorTable(
			1, // root param 1
			m_transformDescHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to root param 3: depth map texture
	{
		list->SetDescriptorHeaps(1, &depthLightSrvHeap);
		list->SetGraphicsRootDescriptorTable(
			3, // root param 3
			depthLightSrvHeap->GetGPUDescriptorHandleForHeapStart());
	}

	// bind to root param 2: material
	// draw call
	{
		list->SetDescriptorHeaps(1, m_materialDescHeap.GetAddressOf());

		const auto cbvSrvIncSize = Resource::instance()->getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
		auto materialH = m_materialDescHeap->GetGPUDescriptorHandleForHeapStart();
		UINT indexOffset = 0;

		for (const auto& m : m_materials)
		{
			list->SetGraphicsRootDescriptorTable(
				2, // root param 2
				materialH);

			constexpr UINT kInstanceCount = 2; // [0] mesh, [1] shadow
			list->DrawIndexedInstanced(m.indicesNum, kInstanceCount, indexOffset, 0, 0);

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
	ThrowIfFalse(m_shadowVsBlob == nullptr);

	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVs",
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf()
	);

	if (FAILED(ret))
	{
		Debug::outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);


	ret = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"MrtWithShadowMapPs",
		Constant::kPsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_psBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf()
	);

	if (FAILED(ret))
	{
		Debug::outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);


	ret = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"shadowVs",
		Constant::kVsShaderModel,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_shadowVsBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf());

	if (FAILED(ret))
	{
		Debug::outputDebugMessage(errorBlob.Get());
	}
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createPipelineState()
{
	ThrowIfFalse(m_pipelineState == nullptr);
	ThrowIfFalse(m_shadowPipelineState == nullptr);

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
		gpipeDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		gpipeDesc.InputLayout = { kInputLayout, static_cast<UINT>(_countof(kInputLayout)) };
		gpipeDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		gpipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		gpipeDesc.NumRenderTargets = 3;
		gpipeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		gpipeDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
		gpipeDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;
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

	// for shadow
	{

		gpipeDesc.VS = { m_shadowVsBlob->GetBufferPointer(), m_shadowVsBlob->GetBufferSize() };
		gpipeDesc.PS = { nullptr, 0 };
		gpipeDesc.NumRenderTargets = 0;
		gpipeDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
		gpipeDesc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
		gpipeDesc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
	}

	ret = Resource::instance()->getDevice()->CreateGraphicsPipelineState(
		&gpipeDesc,
		IID_PPV_ARGS(m_shadowPipelineState.ReleaseAndGetAddressOf()));
	ThrowIfFailed(ret);

	return S_OK;
}

HRESULT PmdActor::createRootSignature(ComPtr<ID3D12RootSignature>* rootSignature)
{
	ThrowIfFalse(rootSignature != nullptr);
	ThrowIfFalse(rootSignature->Get() == nullptr);

	const D3D12_DESCRIPTOR_RANGE descTblRange[] = {
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 1 /* register space */),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0),
		CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4),
	};

	const D3D12_ROOT_PARAMETER rootParams[] = {
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &descTblRange[0],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &descTblRange[1],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 2,
				.pDescriptorRanges = &descTblRange[2],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
		{
			.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
			.DescriptorTable = {
				.NumDescriptorRanges = 1,
				.pDescriptorRanges = &descTblRange[4],
			},
			.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
		},
	};

	D3D12_STATIC_SAMPLER_DESC samplerDescs[] = {
		CD3DX12_STATIC_SAMPLER_DESC(0, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT),
		CD3DX12_STATIC_SAMPLER_DESC(1, D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT),
		CD3DX12_STATIC_SAMPLER_DESC(2,
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP),
	};
	{
		samplerDescs[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDescs[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDescs[1].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDescs[1].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDescs[2].MaxAnisotropy = 1;
		samplerDescs[2].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	}

	const D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
		.NumParameters = 4,
		.pParameters = &rootParams[0],
		.NumStaticSamplers = 3,
		.pStaticSamplers = &samplerDescs[0],
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
	};

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto ret = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		rootSigBlob.GetAddressOf(),
		errorBlob.GetAddressOf());

	if (FAILED(ret))
	{
		Debug::outputDebugMessage(errorBlob.Get());
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

ID3D12PipelineState* PmdActor::getPipelineState() const
{
	ThrowIfFalse(m_pipelineState != nullptr);
	return m_pipelineState.Get();
}

ID3D12RootSignature* PmdActor::getRootSignature() const
{
	ThrowIfFalse(m_rootSignature != nullptr);
	return m_rootSignature.Get();
}

constexpr D3D12_PRIMITIVE_TOPOLOGY PmdActor::getPrimitiveTopology() const
{
	return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

HRESULT PmdActor::setCommonPipelineConfig(ID3D12GraphicsCommandList* list) const
{
	setViewportScissor(Config::kWindowWidth, Config::kWindowHeight);
	list->IASetPrimitiveTopology(getPrimitiveTopology());
	list->IASetVertexBuffers(0, 1, &m_vbView);
	list->IASetIndexBuffer(&m_ibView);

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

		std::vector<PMDVertexForLoader> vertices; // unaligned to 4 bytes
		{
			vertices.resize(m_vertNum * sizeof(PMDVertexForLoader));
			ThrowIfFalse(fread(vertices.data(), vertices.size(), 1, fp) == 1);
		}

		m_vertices.resize(m_vertNum * sizeof(PmdVertexForDx)); // should be aligned to 4 bytes

		for (size_t i = 0; i < m_vertNum; ++i)
		{
			m_vertices.at(i).pos = vertices.at(i).pos;
			m_vertices.at(i).normal = vertices.at(i).normal;
			m_vertices.at(i).uv = vertices.at(i).uv;
			m_vertices.at(i).boneNo[0] = vertices.at(i).boneNo[0];
			m_vertices.at(i).boneNo[1] = vertices.at(i).boneNo[1];
			m_vertices.at(i).boneWeight = vertices.at(i).boneWeight;
			m_vertices.at(i).edgeFlag = vertices.at(i).edgeFlag;
		}

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
					std::string toonFilePath = kToonDir + "/";
					char toonFileName[16] = "";

					int32_t ret = sprintf_s(
						toonFileName,
						"toon%02d.bmp",
						pmdMaterials[i].toonIdx + 1);
					ThrowIfFalse(ret != -1);

					toonFilePath += toonFileName;

					ThrowIfFailed(Loader::instance()->loadImageFromFile(toonFilePath, m_toonResources[i]));
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
					ThrowIfFailed(Loader::instance()->loadImageFromFile(texFilePath, m_textureResources[i]));
				}

				if (!sphFileName.empty())
				{
					const auto sphFilePath = getTexturePathFromModelAndTexPath(modelPath, sphFileName.c_str());
					ThrowIfFailed(Loader::instance()->loadImageFromFile(sphFilePath, m_sphResources[i]));
				}

				if (!spaFileName.empty())
				{
					const auto spaFilePath = getTexturePathFromModelAndTexPath(modelPath, spaFileName.c_str());
					ThrowIfFailed(Loader::instance()->loadImageFromFile(spaFilePath, m_spaResources[i]));
				}
			}
		}

		// load bones
		{
			m_boneNameArray.resize(pmdBones.size());
			m_boneNodeAddressArray.resize(pmdBones.size());
			m_kneeIdxes.clear();

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

				const std::string boneName = pb.boneName;

				if (boneName.find("ひざ") != std::string::npos)
				{
					m_kneeIdxes.emplace_back(i);
				}
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

	Debug::debugOutputFormatString("Vertex num  : %d\n", m_vertNum);
	Debug::debugOutputFormatString("Index num   : %d\n", m_indicesNum);
	Debug::debugOutputFormatString("Material num: %zd\n", m_materials.size());
	Debug::debugOutputFormatString("Bone num    : %zd\n", m_boneMatrices.size());

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
			Debug::debugOutputFormatString("IK bone # = %d : %s\n", ik.boneIdx, getNameFromIdx(ik.boneIdx).c_str());

			for (const auto& node : ik.nodeIdxes)
			{
				Debug::debugOutputFormatString("\tNode bone = %d : %s\n", node, getNameFromIdx(node).c_str());
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

		uint32_t morphCount = 0;
		ThrowIfFalse(fread(&morphCount, sizeof(morphCount), 1, fp) == 1);

		std::vector<VMDMorph> morphs(morphCount);
		fread(morphs.data(), sizeof(VMDMorph), morphCount, fp);

		uint32_t vmdCameraCount = 0;
		ThrowIfFalse(fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp) == 1);

		std::vector<VMDCamera> cameraData(vmdCameraCount);
		ThrowIfFalse(fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp) == vmdCameraCount);

		uint32_t vmdLightCount = 0;
		ThrowIfFalse(fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp) == 1);

		std::vector<VMDLight> lights(vmdLightCount);
		ThrowIfFalse(fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp) == vmdLightCount);

		uint32_t selfShadowCount = 0;
		ThrowIfFalse(fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp) == 1);

		std::vector<VMDSelfShadow> selfShadowData(selfShadowCount);
		ThrowIfFalse(fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp) == selfShadowCount);

		uint32_t ikSwitchCount = 0;
		ThrowIfFalse(fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp) <= 1);

		m_ikEnableData.resize(ikSwitchCount);

		for (auto& ikEnable : m_ikEnableData)
		{
			ThrowIfFalse(fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp) == 1);

			// visibility flag won't be used
			uint8_t visibleFlg = 0;
			ThrowIfFalse(fread(&visibleFlg, sizeof(visibleFlg), 1, fp) == 1);

			uint32_t ikBoneCount = 0;
			ThrowIfFalse(fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp) == 1);

			for (uint32_t i = 0; i < ikBoneCount; ++i)
			{
				char ikBoneName[20] = "";
				ThrowIfFalse(fread(ikBoneName, _countof(ikBoneName), 1, fp) == 1);

				uint8_t flg = 0;
				ThrowIfFalse(fread(&flg, sizeof(flg), 1, fp) == 1);

				ikEnable.ikEnableTable[ikBoneName] = flg;
			}
		}

		for (const VMDMotion& vmdMotion : vmdMotionData)
		{
			m_motionData[vmdMotion.boneName].emplace_back(
				Motion(
					vmdMotion.frameNo,
					DirectX::XMLoadFloat4(&vmdMotion.quaternion),
					vmdMotion.location,
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

		Debug::debugOutputFormatString("Motion num  : %d\n", motionDataNum);
		Debug::debugOutputFormatString("Duration    : %d\n", m_duration);
	}
	ThrowIfFalse(fclose(fp) == 0);

	return S_OK;
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

		XMMATRIX rotation = DirectX::XMMatrixIdentity();
		XMVECTOR offset = XMLoadFloat3(&rit->offset);
		auto it = rit.base();

		if (it != motions.end())
		{
			// interpolation with Bezier curve
			float t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);
			t = getYfromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
			offset = DirectX::XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		const BoneNode node = m_boneNodeTable[boneMotion.first];
		const XMMATRIX mat = DirectX::XMMatrixTranslation(-node.startPos.x, -node.startPos.y, -node.startPos.z)
			* rotation
			* DirectX::XMMatrixTranslation(node.startPos.x, node.startPos.y, node.startPos.z);

		m_boneMatrices[node.boneIdx] = mat * DirectX::XMMatrixTranslationFromVector(offset);
	}

	recursiveMatrixMultiply(m_boneNodeTable["センター"], DirectX::XMMatrixIdentity());

	IKSolve(frameNo);

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

void PmdActor::IKSolve([[maybe_unused]] uint32_t frameNo)
{
	const auto it = find_if(
		m_ikEnableData.rbegin(),
		m_ikEnableData.rend(),
		[frameNo](const VMDIkEnable& ikEnable)
		{
			return ikEnable.frameNo <= frameNo;
		});

	for (const PmdIk& ik : m_pmdIks)
	{
		if (it != m_ikEnableData.rend())
		{
			const auto ikEnableIt = it->ikEnableTable.find(m_boneNameArray[ik.boneIdx]);

			if (ikEnableIt != it->ikEnableTable.end())
			{
				if (!ikEnableIt->second)
					continue;
			}
		}

		const size_t childrenNodesCount = ik.nodeIdxes.size();

		switch (childrenNodesCount)
		{
		case 0:
			ThrowIfFalse(false);
			continue;
		case 1:
			solveLookAt(ik);
			break;
		case 2:
			solveCosineIK(ik);
			break;
		default:
			solveCCDIK(ik);
			break;
		}
	}
}

void PmdActor::solveLookAt(const PmdIk& ik)
{
	using namespace DirectX;

	const BoneNode* rootNode = m_boneNodeAddressArray[ik.nodeIdxes[0]];
	const BoneNode* targetNode = m_boneNodeAddressArray[ik.boneIdx];

	const XMVECTOR rpos1 = DirectX::XMLoadFloat3(&rootNode->startPos);
	const XMVECTOR tpos1 = DirectX::XMLoadFloat3(&targetNode->startPos);

	const XMVECTOR rpos2 = DirectX::XMVector3TransformCoord(rpos1, m_boneMatrices[ik.nodeIdxes[0]]);
	const XMVECTOR tpos2 = DirectX::XMVector3TransformCoord(tpos1, m_boneMatrices[ik.boneIdx]);

	XMVECTOR originVec = DirectX::XMVectorSubtract(tpos1, rpos1);
	XMVECTOR targetVec = DirectX::XMVectorSubtract(tpos2, rpos2);
	originVec = DirectX::XMVector3Normalize(originVec);
	targetVec = DirectX::XMVector3Normalize(targetVec);

	m_boneMatrices[ik.nodeIdxes[0]] = lookAtMatrix(
		originVec,
		targetVec,
		DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3(1, 0, 0));
}

void PmdActor::solveCosineIK(const PmdIk& ik)
{
	using namespace DirectX;

	// offset bone
	const BoneNode* endNode = m_boneNodeAddressArray[ik.targetIdx];

	// intermidiate & root bones
	std::vector<XMVECTOR> positions;
	positions.emplace_back(DirectX::XMLoadFloat3(&endNode->startPos));

	for (uint16_t chainBoneIdx : ik.nodeIdxes)
	{
		const BoneNode* boneNode = m_boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(DirectX::XMLoadFloat3(&boneNode->startPos));
	}

	// reverse orders for simplicity
	std::reverse(positions.begin(), positions.end());

	// store lengths
	std::array<float, 2> edgeLens;
	edgeLens[0] = XMVector3Length(DirectX::XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(DirectX::XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	positions[0] = DirectX::XMVector3Transform(positions[0], m_boneMatrices[ik.nodeIdxes[1]]); // root bone
	// positions[1] will be automatically calculated
	positions[2] = DirectX::XMVector3Transform(positions[2], m_boneMatrices[ik.boneIdx]); // offset bone

	XMVECTOR linearVec = DirectX::XMVectorSubtract(positions[2], positions[0]);
	const float A = DirectX::XMVector3Length(linearVec).m128_f32[0];
	const float B = edgeLens[0];
	const float C = edgeLens[1];

	linearVec = DirectX::XMVector3Normalize(linearVec);

	const float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));
	const float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	XMVECTOR axis = { };

	if (find(m_kneeIdxes.begin(), m_kneeIdxes.end(), ik.nodeIdxes[0]) == m_kneeIdxes.end())
	{
		const BoneNode* targetNode = m_boneNodeAddressArray[ik.boneIdx];
		const XMVECTOR targetPos = DirectX::XMVector3Transform(
			DirectX::XMLoadFloat3(&targetNode->startPos),
			m_boneMatrices[ik.boneIdx]);

		const XMVECTOR vm = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(positions[2], positions[0]));
		const XMVECTOR vt = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetPos, positions[0]));
		axis = DirectX::XMVector3Cross(vt, vm);
	}
	else
	{
		const auto right = XMFLOAT3(1, 0, 0);
		axis = DirectX::XMLoadFloat3(&right);
	}

	XMMATRIX mat1 = DirectX::XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= DirectX::XMMatrixRotationAxis(axis, theta1);
	mat1 *= DirectX::XMMatrixTranslationFromVector(positions[0]);

	XMMATRIX mat2 = DirectX::XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= DirectX::XMMatrixRotationAxis(axis, theta2 - XM_PI);
	mat2 *= DirectX::XMMatrixTranslationFromVector(positions[1]);

	m_boneMatrices[ik.nodeIdxes[1]] *= mat1;
	m_boneMatrices[ik.nodeIdxes[0]] = mat2 * m_boneMatrices[ik.nodeIdxes[1]];
	m_boneMatrices[ik.targetIdx] = m_boneMatrices[ik.nodeIdxes[0]];
}

void PmdActor::solveCCDIK(const PmdIk& ik)
{
	using namespace DirectX;

	const BoneNode* targetBoneNode = m_boneNodeAddressArray[ik.boneIdx];
	const XMVECTOR targetOriginPos = DirectX::XMLoadFloat3(&targetBoneNode->startPos);

	const XMMATRIX parentMat = m_boneMatrices[m_boneNodeAddressArray[ik.boneIdx]->ikParentBone];
	XMVECTOR det = { };
	const XMMATRIX invParentMat = DirectX::XMMatrixInverse(&det, parentMat);
	const XMVECTOR targetNextPos = DirectX::XMVector3Transform(targetOriginPos, m_boneMatrices[ik.boneIdx] * invParentMat);

	XMVECTOR endPos = XMLoadFloat3(&m_boneNodeAddressArray[ik.targetIdx]->startPos);

	std::vector<XMVECTOR> bonePositions;

	for (uint16_t cidx : ik.nodeIdxes)
	{
		bonePositions.emplace_back(XMLoadFloat3(&m_boneNodeAddressArray[cidx]->startPos));
	}

	std::vector<XMMATRIX> mats(bonePositions.size());
	std::fill(mats.begin(), mats.end(), DirectX::XMMatrixIdentity());

	constexpr float epsilon = 0.0005f;
	const float ikLimit = ik.limit * XM_PI;

	for (int32_t c = 0; c < ik.iterations; ++c)
	{
		// stop processing if we are almost close to target position
		if (XMVector3Length(DirectX::XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
			break;

		for (int32_t bidx = 0; bidx < bonePositions.size(); ++bidx)
		{
			const XMVECTOR& pos = bonePositions[bidx];

			const XMVECTOR vecToEnd = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(endPos, pos));
			const XMVECTOR vecToTarget = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(targetNextPos, pos));

			// move to next bone if both vectors are almost same (since we couldn't do cross operation)
			if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon)
				continue;

			const XMVECTOR cross = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vecToEnd, vecToTarget));
			float angle = DirectX::XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];
			angle = std::min(angle, ikLimit);

			const XMMATRIX rot = DirectX::XMMatrixRotationAxis(cross, angle);

			XMMATRIX mat = DirectX::XMMatrixTranslationFromVector(-pos)
				* rot
				* DirectX::XMMatrixTranslationFromVector(pos);

			mats[bidx] *= mat;

			for (int32_t idx = bidx - 1; idx >= 0; --idx)
			{
				bonePositions[idx] = DirectX::XMVector3Transform(bonePositions[idx], mat);
			}

			endPos = DirectX::XMVector3Transform(endPos, mat);

			if (DirectX::XMVector3Length(DirectX::XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
				break;
		}
	}

	{
		int32_t idx = 0;

		for (uint16_t cidx : ik.nodeIdxes)
		{
			m_boneMatrices[cidx] = mats[idx];
			++idx;
		}

		BoneNode* rootNode = m_boneNodeAddressArray[ik.nodeIdxes.back()];
		recursiveMatrixMultiply(*rootNode, parentMat);
	}
}

static HRESULT setViewportScissor(int32_t width, int32_t height)
{
	const D3D12_VIEWPORT viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	Resource::instance()->getCommandList()->RSSetViewports(1, &viewport);

	const D3D12_RECT scissorRect = CD3DX12_RECT(0, 0, width, height);
	Resource::instance()->getCommandList()->RSSetScissorRects(1, &scissorRect);

	return S_OK;
}

static std::string getModelPath(PmdActor::Model model)
{
	switch (model) {
	case PmdActor::Model::kMiku: return kModelDir + "/" + "初音ミク.pmd";
	case PmdActor::Model::kMikuMetal: return kModelDir + "/" + "初音ミクmetal.pmd";
	case PmdActor::Model::kLuka: return kModelDir + "/" + "巡音ルカ.pmd";
	case PmdActor::Model::kLen: return kModelDir + "/" + "鏡音レン.pmd";
	case PmdActor::Model::kKaito: return kModelDir + "/" + "カイト.pmd";
	case PmdActor::Model::kHaku: return kModelDir + "/" + "弱音ハク.pmd";
	case PmdActor::Model::kRin: return kModelDir + "/" + "鏡音リン.pmd";
	case PmdActor::Model::kMeiko: return kModelDir + "/" + "咲音メイコ.pmd";
	case PmdActor::Model::kNeru: return kModelDir + "/" + "亞北ネル.pmd";
	default: ThrowIfFalse(false); break;
	}

	return "";
}

std::string getMotionPath()
{
	//return kMotionDir + "/" + "pose.vmd";
	//return kMotionDir + "/" + "swing.vmd";
	return kMotionDir + "/" + "motion.vmd";
	//return kMotionDir + "/" + "squat.vmd";
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
			vbView.StrideInBytes = static_cast<UINT>(Util::alignmentedSize(sizeof(PmdVertexForDx), 4));
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

static DirectX::XMMATRIX lookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
	return DirectX::XMMatrixTranspose(lookAtMatrix(origin, up, right)) * lookAtMatrix(lookat, up, right);
}

static DirectX::XMMATRIX lookAtMatrix(const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
	using namespace DirectX;

	const XMVECTOR& vz = lookat;
	XMVECTOR vy = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));
	XMVECTOR vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));

	vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));

	// if both LookAt and up vectors direct in the same way, vx should be updated based on right vector
	if (std::abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f)
	{
		vx = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&right));
		vy = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vz, vx));
		vx = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(vy, vz));
	}

	XMMATRIX ret = DirectX::XMMatrixIdentity();
	ret.r[0] = vx;
	ret.r[1] = vy;
	ret.r[2] = vz;

	return ret;
}

