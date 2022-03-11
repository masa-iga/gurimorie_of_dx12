#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#pragma warning(pop)

class TimeStamp
{
public:
	enum class Index
	{
		k0,
		k1,
		kEnd,
	};

	HRESULT init();
	void clear();
	void set(ID3D12GraphicsCommandList* list, Index index);
	void resolve(ID3D12GraphicsCommandList* list);
	float getInUsec(Index index0, Index index1);

private:
	static constexpr size_t kNumOfTimestamp = static_cast<size_t>(Index::kEnd);

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_tsQueryHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_tsResource = nullptr;
	uint64_t m_gpuFreq = 0;
};

