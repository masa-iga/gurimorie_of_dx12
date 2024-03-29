#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cstring>
#include <d3dx12.h>
#include <dxcapi.h>
#include <stdexcept>
#include <Windows.h>
#pragma warning(pop)

namespace Debug {
	void debugOutputFormatString(const char* format, ...);
	void outputDebugMessage(ID3DBlob* errorBlob);
	void outputDebugMessage(IDxcOperationResult* result);
} // namespace Debug

void ThrowIfFailed(HRESULT hr);
void ThrowIfFalse(BOOL b);
