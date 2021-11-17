#pragma once
#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <cstdint>
#include <cstring>
#include <Windows.h>
#include <wrl.h>
#include <map>
#pragma warning(pop)
#include "debug.h"

class Loader
{
public:
	static Loader* instance()
	{
		ThrowIfFalse(m_loader != nullptr);
		return m_loader;
	}

	static void init()
	{
		if (m_loader != nullptr)
			return;

		m_loader = new Loader();
	}

	static void quit()
	{
		if (m_loader == nullptr)
			return;

		delete m_loader;
		m_loader = nullptr;
	}
	HRESULT loadImageFromFile(const std::string& texPath, Microsoft::WRL::ComPtr<ID3D12Resource>& buffer);

private:
	Loader() = default;
	Loader(const Loader&) = delete;
	void operator=(const Loader&) = delete;

	static Loader* m_loader;
	std::map<std::string, ID3D12Resource*> m_resourceTable;
};
