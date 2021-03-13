#include "util.h"
#include "debug.h"

namespace Util {

TimeCounter::TimeCounter(std::string str)
	: m_str(str)
{
	ThrowIfFalse(QueryPerformanceCounter(&m_start));
}

TimeCounter::~TimeCounter()
{
	LARGE_INTEGER end = { };
	ThrowIfFalse(QueryPerformanceCounter(&end));

	LARGE_INTEGER freq = { };
	ThrowIfFalse(QueryPerformanceFrequency(&freq));

	auto elapsed = end.QuadPart - m_start.QuadPart;
	elapsed *= 1'000'000; // to usec
	elapsed /= freq.QuadPart;

	if (m_str.empty())
		DebugOutputFormatString("%zd usec\n", elapsed);
	else
		DebugOutputFormatString("%s %zd usec\n", m_str.c_str(), elapsed);
}

size_t Util::alignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - (size % alignment);
}

} // namespace Util
