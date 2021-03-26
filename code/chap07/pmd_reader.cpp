#include "pmd_reader.h"
#include <cstdio>
#include <d3dx12.h>
#include <DirectXMath.h>
#include <vector>
#include "debug.h"
#include "init.h"

struct PMDHeader
{
	float version = 0.0f;
	char model_name[20] = { };
	char comment[256] = { };
};

struct PMDVertex
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 uv;
	UINT16 boneNo[2] = { };
	UINT8 boneWeight = 0;
	UINT8 edgeFlag = 0;
};

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

std::vector<UINT8> s_vertices;
static ID3D12Resource *s_vertResource = nullptr;
static D3D12_VERTEX_BUFFER_VIEW s_vbView = { };

static HRESULT createResource(size_t width);

std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> PmdReader::getInputElementDesc()
{
	return { kInputLayout, static_cast<UINT>(_countof(kInputLayout)) };
}

HRESULT PmdReader::read()
{
	FILE* fp = nullptr;
	ThrowIfFalse(fopen_s(&fp, "Model/‰‰¹ƒ~ƒN.pmd", "rb") == 0);

	char signature[kNumSignature] = { };
	ThrowIfFalse(fread(signature, sizeof(signature), 1, fp) == 1);
	ThrowIfFalse(strncmp(signature, kSignature, kNumSignature) == 0);

	PMDHeader pmdHeader = { };
	ThrowIfFalse(fread(&pmdHeader, sizeof(pmdHeader), 1, fp) == 1);

	UINT32 vertNum = 0;
	ThrowIfFalse(fread(&vertNum, sizeof(vertNum), 1, fp) == 1);

	s_vertices.resize(vertNum * kPmdVertexSize);
	ThrowIfFalse(fread(s_vertices.data(), s_vertices.size(), 1, fp) == 1);

	ThrowIfFalse(fclose(fp) == 0);


	ThrowIfFailed(createResource(s_vertices.size()));

	D3D12_VERTEX_BUFFER_VIEW vbView = { };
	{
		vbView.BufferLocation = s_vertResource->GetGPUVirtualAddress();
		vbView.SizeInBytes = static_cast<UINT>(s_vertices.size());
		vbView.StrideInBytes = kPmdVertexSize;
	}


	UINT8 *vertMap = nullptr;
	auto ret = s_vertResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&vertMap)
	);
	ThrowIfFailed(ret);

	std::copy(std::begin(s_vertices), std::end(s_vertices), vertMap);

	return S_OK;
}

static HRESULT createResource(size_t width)
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
			IID_PPV_ARGS(&s_vertResource)
		);
		ThrowIfFailed(ret);
	}

	return S_OK;
}
