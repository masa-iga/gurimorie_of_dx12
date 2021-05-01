#include "pmd_reader.h"
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <algorithm>
#include <cstdio>
#include <d3dx12.h>
#include <DirectXTex.h>
#pragma warning(pop)
#include "debug.h"
#include "init.h"
#include "util.h"

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
static_assert(sizeof(PMDVertex) == 38);
#pragma pack()

#pragma pack(1)
	struct PMDMaterial
	{
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		float specularity;
		DirectX::XMFLOAT3 specular;
		DirectX::XMFLOAT3 ambient;
		UINT8 toonIdx;
		UINT8 edgeFlg;
		UINT indicesNum;
		char texFilePath[20];
	};
	static_assert(sizeof(PMDMaterial) == 70);
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
static constexpr size_t kPmdVertexSize = sizeof(PMDVertex);

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

static std::string getTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath);
template<typename T>
static std::tuple<HRESULT, D3D12_VERTEX_BUFFER_VIEW, D3D12_INDEX_BUFFER_VIEW>
createResourcesInternal(ID3D12Resource** ppVertResource, const std::vector<T>& vertices, ID3D12Resource** ppIbResource, const std::vector<UINT16>& indices);
static HRESULT createBufferResource(ID3D12Resource** ppVertResource, size_t width);

PmdReader::PmdReader()
{
	auto ret = createWhiteTexture();
	ThrowIfFailed(ret);

	ret = createBlackTexture();
	ThrowIfFailed(ret);

	ret = createDebugResources();
	ThrowIfFailed(ret);
}

std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> PmdReader::getInputElementDesc()
{
	return { kInputLayout, static_cast<UINT>(_countof(kInputLayout)) };
}

HRESULT PmdReader::readData()
{
	//const std::string strModelPath = "Model/カイト.pmd";
	//const std::string strModelPath = "Model/鏡音リン.pmd";
	//const std::string strModelPath = "Model/鏡音レン.pmd";
	//const std::string strModelPath = "Model/咲音メイコ.pmd";
	//const std::string strModelPath = "Model/弱音ハク.pmd";
	//const std::string strModelPath = "Model/巡音ルカ.pmd";
	const std::string strModelPath = "Model/初音ミク.pmd";
	//const std::string strModelPath = "Model/初音ミクmetal.pmd";
	//const std::string strModelPath = "Model/亞北ネル.pmd";

	FILE* fp = nullptr;
	ThrowIfFalse(fopen_s(&fp, strModelPath.c_str(), "rb") == 0);
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

		// load materials
		{
			UINT materialNum = 0;
			ThrowIfFalse(fread(&materialNum, sizeof(materialNum), 1, fp) == 1);

			std::vector<PMDMaterial> pmdMaterials(materialNum);
			ThrowIfFalse(fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp) == 1);

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
					const auto texFilePath = getTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
					m_textureResources[i] = loadTextureFromFile(texFilePath);
				}

				if (!sphFileName.empty())
				{
					const auto sphFilePath = getTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
					m_sphResources[i] = loadTextureFromFile(sphFilePath);
				}

				if (!spaFileName.empty())
				{
					const auto spaFilePath = getTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
					m_spaResources[i] = loadTextureFromFile(spaFilePath);
				}
			}
		}
	}
	ThrowIfFalse(fclose(fp) == 0);

	DebugOutputFormatString("Vertex num  : %d\n", m_vertNum);
	DebugOutputFormatString("Index num   : %d\n", m_indicesNum);
	DebugOutputFormatString("Material num: %zd\n", m_materials.size());

	return S_OK;
}

HRESULT PmdReader::createResources()
{
	auto [ret, vbView, ibView] = createResourcesInternal(&m_vertResource, m_vertices, &m_ibResource, m_indices);
	ThrowIfFailed(ret);
	m_vbView = vbView;
	m_ibView = ibView;

	ret = createMaterialResrouces();
	ThrowIfFailed(ret);

	return S_OK;
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

const std::vector<Material> PmdReader::getMaterials() const
{
	return m_materials;
}

ID3D12DescriptorHeap* PmdReader::getMaterialDescHeap()
{
	return m_materialDescHeap.Get();
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

Microsoft::WRL::ComPtr<ID3D12Resource> PmdReader::loadTextureFromFile(const std::string& texPath)
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
	ThrowIfFailed(ret);

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

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	{
		ret = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&resource));
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


HRESULT PmdReader::createWhiteTexture()
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
			IID_PPV_ARGS(&m_whiteTextureResource));
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

HRESULT PmdReader::createBlackTexture()
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
			IID_PPV_ARGS(&m_blackTextureResource));
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

HRESULT PmdReader::createGrayGradiationTexture()
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
		IID_PPV_ARGS(&m_grayGradiationTextureResource));
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

HRESULT PmdReader::createDebugResources()
{
	ID3D12Resource* vertResource = nullptr;
	ID3D12Resource* ibResource = nullptr;

	auto [ret, vbView, ibView] = createResourcesInternal(&vertResource, s_debugVertices, &ibResource, s_debugIndices);

	m_debugVbView = vbView;
	m_debugIbView = ibView;

	return S_OK;
}

HRESULT PmdReader::createMaterialResrouces()
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
	ID3D12Resource* materialResource = nullptr;
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
			IID_PPV_ARGS(&materialResource));
		ThrowIfFailed(ret);
	}

	// copy
	{
		UINT8* pMapMaterial = nullptr;
		auto ret = materialResource->Map(0, nullptr, reinterpret_cast<void**>(&pMapMaterial));
		ThrowIfFailed(ret);

		for (const auto& m : m_materials)
		{
			*reinterpret_cast<MaterialForHlsl*>(pMapMaterial) = m.material;
			pMapMaterial += materialBufferSize;
		}

		materialResource->Unmap(0, nullptr);
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

		auto ret = Resource::instance()->getDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_materialDescHeap));
		ThrowIfFailed(ret);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
		{
			cbvDesc.BufferLocation = materialResource->GetGPUVirtualAddress();
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

		auto ret = Resource::instance()->getDevice()->CreateCommittedResource(
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

