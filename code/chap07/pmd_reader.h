#pragma once
#include <d3d12.h>
#include <utility>
#include <vector>

class PmdReader {
public:
	std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> getInputElementDesc();
	HRESULT readData();
	HRESULT createResources();
	const D3D12_VERTEX_BUFFER_VIEW* getVbView() const;
	UINT getVertNum() const;
	const D3D12_INDEX_BUFFER_VIEW* getIbView() const;
	UINT getIndexNum() const;

private:
	std::vector<UINT8> m_vertices;
	std::vector<UINT16> m_indices;
	UINT m_vertNum = 0;
	UINT m_indicesNum = 0;

	ID3D12Resource* m_vertResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	ID3D12Resource* m_ibResource = nullptr;
	D3D12_INDEX_BUFFER_VIEW m_ibView = { };
};
