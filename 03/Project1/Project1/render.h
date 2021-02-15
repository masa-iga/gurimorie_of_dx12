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
	ID3D12Fence* m_pFence = nullptr;
	UINT64 m_fenceVal = 0;
};
