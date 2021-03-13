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

size_t AlignmentedSize(size_t size, size_t alignment);

} // namespace Util

