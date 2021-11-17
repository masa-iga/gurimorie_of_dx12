#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <winnt.h>
#include <cstdint>
#pragma warning(pop)

namespace Constant {
	constexpr size_t kD3D12ConstantBufferAlignment = 256; // bytes
	constexpr LPCSTR kVsShaderModel = "vs_5_0";
	constexpr LPCSTR kPsShaderModel = "ps_5_0";
}
