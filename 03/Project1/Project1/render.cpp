#include <cassert>
#include "init.h"
#include "render.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>

#include <string>
#include <wrl.h>
#include <shellapi.h>

HRESULT render()
{
	getCommandListInstance()->Close();

	auto ret = getCommandAllocatorInstance()->Reset();
	assert(ret == S_OK);

	return S_OK;
}