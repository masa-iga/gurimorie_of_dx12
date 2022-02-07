#include "pixif.h"

void PixIf::setMarker(ID3D12GraphicsCommandList* list, PCSTR formatString, UINT64 color)
{
	PIXSetMarker(list, color, formatString);
}

void PixIf::beginEvent(ID3D12GraphicsCommandList* list, PCSTR formatString, UINT64 color)
{
	PIXBeginEvent(list, color, formatString);
}

void PixIf::endEvent(ID3D12GraphicsCommandList* list)
{
	PIXEndEvent(list);
}

PixScopedEvent::PixScopedEvent(ID3D12GraphicsCommandList* list, PCSTR formatString, UINT64 color)
	: m_list(list)
{
	PixIf::beginEvent(list, formatString, color);
}

PixScopedEvent::~PixScopedEvent()
{
	PixIf::endEvent(m_list);
}

