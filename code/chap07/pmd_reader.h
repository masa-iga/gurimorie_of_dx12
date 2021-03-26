#pragma once
#include <d3d12.h>
#include <utility>

namespace PmdReader {

std::pair<const D3D12_INPUT_ELEMENT_DESC*, UINT> getInputElementDesc();
HRESULT read();

} // namespace PMdReader
