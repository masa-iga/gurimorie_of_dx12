#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)
#include <cstdint>

class TimeStamp
{
public:
	enum class Index
	{
		k0,
		k1,
	};

	HRESULT init();
	void set(Index index);
	void resolve();
	void get(Index index0, Index index1);

private:
	static constexpr size_t kNumOfTimestamp = 2;

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_tsQueryHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tsResource = nullptr;
	uint64_t m_gpuFreq = 0;
};

