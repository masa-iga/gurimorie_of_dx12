#include "pmd_reader.h"
#include <cstdio>
#include <d3dx12.h>
#include <DirectXMath.h>
#include "debug.h"
#include "init.h"

struct PMDHeader
{
	float version = 0.0f;
	char model_name[20] = { };
	char comment[256] = { };
};

#pragma pack(1) // size of the struct is 38 bytes, so need to prevent padding
struct PMDVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	UINT16 boneNo[2] = { };
	UINT8 boneWeight = 0;
	UINT8 edgeFlag = 0;
};
#pragma pack()

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
		"BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0,
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

static constexpr char kSignature[] = "Pmd";
static constexpr size_t kNumSignature = 3;
static constexpr size_t kPmdVertexSize = 38;
static_assert(sizeof(PMDVertex) == kPmdVertexSize);

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

const std::vector<UINT16> s_debugIndices = { 0, 1, 2, 3, 4, 5 };

template<typename T>
static std::tuple<HRESULT, D3D12_VERTEX_BUFFER_VIEW, D3D12_INDEX_BUFFER_VIEW>
createResourcesInternal(ID3D12Resource** ppVertResource, const std::vector<T>& vertices, ID3D12Resource** ppIbResource, const std::vector<UINT16>& indices);
static HRESULT createBufferResource(ID3D12Resource** ppVertResource, size_t width);

PmdReader::PmdReader()
{
	auto ret = createDebugResources();
	ThrowIfFailed(ret);
}

std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> PmdReader::getInputElementDesc()
{
	return { kInputLayout, static_cast<UINT>(_countof(kInputLayout)) };
}

HRESULT PmdReader::readData()
{
	FILE* fp = nullptr;
	ThrowIfFalse(fopen_s(&fp, "Model/èââπÉ~ÉN.pmd", "rb") == 0);
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
	}
	ThrowIfFalse(fclose(fp) == 0);

	return S_OK;
}

HRESULT PmdReader::createResources()
{
	auto [ret, vbView, ibView] = createResourcesInternal(&m_vertResource, m_vertices, &m_ibResource, m_indices);

	m_vbView = vbView;
	m_ibView = ibView;
	return ret;
}

const D3D12_VERTEX_BUFFER_VIEW* PmdReader::getVbView() const
{
	return &m_vbView;
}

UINT PmdReader::getVertNum() const
{
	return m_vertNum;
}

const D3D12_INDEX_BUFFER_VIEW* PmdReader::getIbView() const
{
	return &m_ibView;
}

UINT PmdReader::getIndexNum() const
{
	return m_indicesNum;
}

const D3D12_VERTEX_BUFFER_VIEW* PmdReader::getDebugVbView() const
{
	return &m_debugVbView;
}

const D3D12_INDEX_BUFFER_VIEW* PmdReader::getDebugIbView() const
{
	return &m_debugIbView;
}

UINT PmdReader::getDebugIndexNum() const
{
	return static_cast<UINT>(s_debugIndices.size());
}

HRESULT PmdReader::createDebugResources()
{
	ID3D12Resource* vertResource = nullptr;
	ID3D12Resource* ibResource = nullptr;

	auto [ret, vbView, ibView] = createResourcesInternal(&vertResource, s_debugVertices, &ibResource, s_debugIndices);

	m_debugVbView = vbView;
	m_debugIbView = ibView;

	return S_OK;
}

template<typename T>
static std::tuple<HRESULT, D3D12_VERTEX_BUFFER_VIEW, D3D12_INDEX_BUFFER_VIEW>
createResourcesInternal(ID3D12Resource** ppVertResource, const std::vector<T>& vertices, ID3D12Resource** ppIbResource, const std::vector<UINT16>& indices)
{
	ThrowIfFalse(ppVertResource != nullptr);
	ThrowIfFalse(ppIbResource != nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	{
		const size_t sizeInBytes = vertices.size() * sizeof(vertices[0]);

		ThrowIfFailed(createBufferResource(ppVertResource, sizeInBytes));
		ThrowIfFalse((*ppVertResource) != nullptr);
		{
			vbView.BufferLocation = (*ppVertResource)->GetGPUVirtualAddress();
			vbView.SizeInBytes = static_cast<UINT>(sizeInBytes);
			vbView.StrideInBytes = kPmdVertexSize;
		}

		T* vertMap = nullptr;
		auto ret = (*ppVertResource)->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&vertMap)
		);
		ThrowIfFailed(ret);

		std::copy(std::begin(vertices), std::end(vertices), vertMap);

		(*ppVertResource)->Unmap(0, nullptr);
	}

	D3D12_INDEX_BUFFER_VIEW ibView = { };
	{
		const size_t sizeInBytes = indices.size() * sizeof(indices[0]);

		ThrowIfFailed(createBufferResource(ppIbResource, sizeInBytes));
		ThrowIfFalse((*ppIbResource) != nullptr);
		{
			ibView.BufferLocation = (*ppIbResource)->GetGPUVirtualAddress();
			ibView.SizeInBytes = static_cast<UINT>(sizeInBytes);
			ibView.Format = DXGI_FORMAT_R16_UINT;
		}

		UINT16* ibMap = nullptr;
		auto ret = (*ppIbResource)->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&ibMap));
		ThrowIfFailed(ret);

		std::copy(std::begin(indices), std::end(indices), ibMap);

		(*ppIbResource)->Unmap(0, nullptr);
	}

	return { S_OK, vbView , ibView };
}

static HRESULT createBufferResource(ID3D12Resource** ppResource, size_t width)
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

		auto ret = getInstanceOfDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(ppResource)
		);
		ThrowIfFailed(ret);
	}

	return S_OK;
}

