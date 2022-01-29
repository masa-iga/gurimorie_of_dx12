#include "timestamp.h"
#include "debug.h"
#include "init.h"
#include "util.h"

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

		result = m_tsQueryHeap.Get()->SetName(Util::getWideStringFromString("timestampQueryHeap").c_str());
		ThrowIfFailed(result);
	}

	{
		const D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * kNumOfTimestamp);
		const D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);

		auto result = Resource::instance()->getDevice()->CreateCommittedResource(
			&heapProp,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(m_tsResource.ReleaseAndGetAddressOf()));
		ThrowIfFailed(result);

		result = m_tsResource.Get()->SetName(Util::getWideStringFromString("timestampResource").c_str());
		ThrowIfFailed(result);
	}

	{
		Resource::instance()->getCommandQueue()->GetTimestampFrequency(&m_gpuFreq);
		ThrowIfFalse(m_gpuFreq >= 1'000'000); // The counter frequency must be over 1 MHz

		Debug::debugOutputFormatString("Time stamp freq: %zd Hz\n", m_gpuFreq);
	}

	clear();

	return S_OK;
}

void TimeStamp::clear()
{
	uint64_t* pData = nullptr;

	auto result = m_tsResource->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&pData));
	ThrowIfFailed(result);

	for (uint32_t i = 0; i < kNumOfTimestamp; ++i)
	{
		pData[i] = 0;
	}

	m_tsResource->Unmap(0, nullptr);
}

void TimeStamp::set(ID3D12GraphicsCommandList* list, Index index)
{
	ThrowIfFalse(static_cast<size_t>(index) < kNumOfTimestamp);
	list->EndQuery(m_tsQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(index));
}

void TimeStamp::resolve(ID3D12GraphicsCommandList* list)
{
	constexpr uint32_t startIndex = 0;

	list->ResolveQueryData(
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

	//debugOutputFormatString("[%d %d] %6.1f us (%zd %zd %zd)\n", idx0, idx1, time_us, m_gpuFreq, pData[idx1], pData[idx0]);
	return time_us;
}

