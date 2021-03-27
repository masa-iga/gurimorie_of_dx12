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

private:
	std::vector<UINT8> m_vertices;
	ID3D12Resource* m_vertResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vbView = { };
	UINT m_vertNum = 0;
};
