#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3dx12.h>
#include <pix3.h>
#pragma warning(pop)

class PixIf
{
public:
	static constexpr UINT64 kDefaultColor = 0x0;

	static void setMarker(ID3D12GraphicsCommandList* list, PCSTR formatString, UINT64 color = kDefaultColor);
	static void beginEvent(ID3D12GraphicsCommandList* list,PCSTR formatString, UINT64 color = kDefaultColor);
	static void endEvent(ID3D12GraphicsCommandList* list);
};

class PixScopedEvent {
public:
	static constexpr UINT64 kDefaultColor = PixIf::kDefaultColor;

	PixScopedEvent(ID3D12GraphicsCommandList* list, PCSTR formatString, UINT64 color = kDefaultColor);
	~PixScopedEvent();

private:
	ID3D12GraphicsCommandList* m_list = nullptr;
};

