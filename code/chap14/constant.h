#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cstdint>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <winnt.h>
#pragma warning(pop)

namespace Constant {
	constexpr size_t kD3D12ConstantBufferAlignment = 256; // bytes
	constexpr D3D_SHADER_MACRO* kCompileShaderDefines = nullptr;
	constexpr LPCSTR kVsShaderModel = "vs_5_1";
	constexpr LPCSTR kPsShaderModel = "ps_5_1";
	constexpr UINT kCompileShaderFlags1 = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
	constexpr UINT kCompileShaderFlags2 = 0;
	constexpr LPCWSTR kDxcVsShaderModel = L"vs_6_6";
	constexpr LPCWSTR kDxcPsShaderModel = L"ps_6_6";
	constexpr DXGI_FORMAT kDefaultRtFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr D3D_ROOT_SIGNATURE_VERSION kRootSignatureVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
}

