#include "timestamp.h"
#include "debug.h"
#include "init.h"

HRESULT TimeStamp::init()
{
	{
		D3D12_QUERY_HEAP_DESC heapDesc = { };
		{
			heapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
			heapDesc.Count = kNumOfTimestamp;
			heapDesc.NodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateQueryHeap(
			&heapDesc,
			IID_PPV_ARGS(m_tsQueryHeap.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		D3D12_RESOURCE_DESC resourceDesc = { };
		{
			resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			resourceDesc.Alignment = 0;
			resourceDesc.Width = sizeof(UINT64) * kNumOfTimestamp;
			resourceDesc.Height = 1;
			resourceDesc.DepthOrArraySize = 1;
			resourceDesc.MipLevels = 1;
			resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			resourceDesc.SampleDesc = { 1, 0 };
			resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		}

		D3D12_HEAP_PROPERTIES heapProp = { };
		{
			heapProp.Type = D3D12_HEAP_TYPE_READBACK;
			heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProp.CreationNodeMask = 0;
			heapProp.VisibleNodeMask = 0;
		}

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(m_tsResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);
	}

	{
		Resource::instance()->getCommandQueue()->GetTimestampFrequency(&m_gpuFreq);
		ThrowIfFalse(m_gpuFreq >= 1'000'000); // The counter frequency must be over 1 MHz
	}

	return S_OK;
}

void TimeStamp::set(Index index)
{
	ThrowIfFalse(static_cast<size_t>(index) < kNumOfTimestamp);
	Resource::instance()->getCommandList()->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(index));
}

void TimeStamp::resolve()
{
	constexpr uint32_t startIndex = 0;

	Resource::instance()->getCommandList()->ResolveQueryData(
		m_tsQueryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		startIndex,
		kNumOfTimestamp,
		m_tsResource.Get(),
		0);
}

float TimeStamp::getInUsec(Index index0, Index index1)
{
	const size_t idx0 = static_cast<size_t>(index0);
	const size_t idx1 = static_cast<size_t>(index1);

	uint64_t* pData = nullptr;

	auto result = m_tsResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&pData));
	ThrowIfFailed(result);

	const uint64_t elaps = pData[idx0] >= pData[idx1] ?
		pData[idx0] - pData[idx1] :
		pData[idx1] - pData[idx0];

	const float time_us = static_cast<float>(elaps) / (m_gpuFreq / 1'000'000);

	m_tsResource->Unmap(0, nullptr);

	//DebugOutputFormatString("%6.1f us (%zd %zd %zd)\n", time_us, m_gpuFreq, pData[idx1], pData[idx0]);
	return time_us;
}

