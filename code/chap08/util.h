#pragma once
#include <Windows.h>
#include <string>

namespace Util {

class TimeCounter
{
public:
	TimeCounter(std::string str = "");
	~TimeCounter();

private:
	LARGE_INTEGER m_start = {};
	std::string m_str = "";
};

size_t alignmentedSize(size_t size, size_t alignment);
std::wstring getWideStringFromString(const std::string& str);
std::string getExtension(const std::string& path);
std::pair<std::string, std::string> splitFileName(const std::string& path, const char splitter);

} // namespace Util

