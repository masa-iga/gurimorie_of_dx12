#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <utility>
#include <vector>

#pragma pack(1)
struct PMDMaterial
{
	DirectX::XMFLOAT3 diffuse;
	float alpha;
	float specularity;
	DirectX::XMFLOAT3 specular;
	DirectX::XMFLOAT3 ambient;
	unsigned char toonIdx;
	unsigned char edgeFlg;
	unsigned int indicesNum;
	char texFilePath[20];
};
static_assert(sizeof(PMDMaterial) == 70);
#pragma pack()

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

	const D3D12_VERTEX_BUFFER_VIEW* getDebugVbView() const;
	const D3D12_INDEX_BUFFER_VIEW* getDebugIbView() const;
	UINT getDebugIndexNum() const;

private:
	HRESULT createDebugResources();

	std::vector<UINT8> m_vertices;
	std::vector<UINT16> m_indices;
	std::vector<PMDMaterial> m_pmdMaterials;
	UINT m_vertNum = 0;
	UINT m_indicesNum = 0;

	ID3D12Resource* m_vertResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	ID3D12Resource* m_ibResource = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };

	D3D12_VERTEX_BUFFER_VIEW m_debugVbView = { };
	D3D12_INDEX_BUFFER_VIEW m_debugIbView = { };
};
