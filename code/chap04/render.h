#pragma once
#include <Windows.h>
#include <d3d12.h>

class Render {
public:
	HRESULT init();
	HRESULT render();
	HRESULT waitForEndOfRendering();
	HRESULT swap();

private:
	HRESULT loadShaders();
	HRESULT setPipelineState();

	ID3DBlob* m_vsBlob = nullptr;
	ID3DBlob* m_psBlob = nullptr;

	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_fenceVal = 0;
};
