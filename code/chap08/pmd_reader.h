#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include <utility>
#include <vector>

struct MaterialForHlsl
{
	DirectX::XMFLOAT3 diffuse;
	float alpha = 0.0f;
	DirectX::XMFLOAT3 specular;
	float specularity = 0.0f;
	DirectX::XMFLOAT3 ambient;
};

struct AdditionalMaterial
{
	std::string texPath;
	INT toonIdx = 0;
	bool edgeFlg = false;
};

struct Material
{
	UINT indicesNum = 0;
	MaterialForHlsl material;
	AdditionalMaterial additional;
};

class PmdReader {
public:
	PmdReader();
	std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> getInputElementDesc();
	HRESULT readData();
	HRESULT createResources();
	const D3D12_VERTEX_BUFFER_VIEW* getVbView() const;
	UINT getVertNum() const;
	const D3D12_INDEX_BUFFER_VIEW* getIbView() const;
	UINT getIndexNum() const;
	ID3D12DescriptorHeap* getMaterialDescHeap();

	const D3D12_VERTEX_BUFFER_VIEW* getDebugVbView() const;
	const D3D12_INDEX_BUFFER_VIEW* getDebugIbView() const;
	UINT getDebugIndexNum() const;

private:
	HRESULT createDebugResources();

	std::vector<UINT8> m_vertices;
	std::vector<UINT16> m_indices;
	std::vector<Material> m_materials;
	UINT m_vertNum = 0;
	UINT m_indicesNum = 0;

	ID3D12Resource* m_vertResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	ID3D12Resource* m_ibResource = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };
	ID3D12DescriptorHeap* m_materialDescHeap = nullptr;

	D3D12_VERTEX_BUFFER_VIEW m_debugVbView = { };
	D3D12_INDEX_BUFFER_VIEW m_debugIbView = { };
};
