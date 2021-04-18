#pragma once
#include <DirectXTex.h>
#include <Windows.h>
#include <functional>
#include <string>
#include <unordered_map>

namespace Util {

using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;

class TimeCounter
{
public:
	TimeCounter(std::string str = "");
	~TimeCounter();

private:
	LARGE_INTEGER m_start = {};
	std::string m_str = "";
};

void init();

size_t alignmentedSize(size_t size, size_t alignment);
std::wstring getWideStringFromString(const std::string& str);
std::string getExtension(const std::string& path);
std::pair<std::string, std::string> splitFileName(const std::string& path, const char splitter);
std::unordered_map<std::string, LoadLambda_t> getLoadLambdaTable();

} // namespace Util

