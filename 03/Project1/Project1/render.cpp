#include <cassert>
#include "init.h"
#include "render.h"

HRESULT render()
{
	auto commandAllocator = getCommandAllocatorInstance();

	auto ret = commandAllocator->Reset();
	assert(ret == S_OK);

	return S_OK;
}