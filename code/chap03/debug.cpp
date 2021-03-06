#include "debug.h"

#ifdef _DEBUG
#include <iostream>
#include <cstdarg>
#endif // _DEBUG

static std::string HrToString(HRESULT hr);

class HrException : public std::runtime_error
{
public:
	HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
	HRESULT Error() const { return m_hr; }

private:
	const HRESULT m_hr;
};

class BlException : public std::runtime_error
{
public:
	BlException(BOOL bl) : std::runtime_error("unexpected result"), m_bl(bl) {}
	BOOL Error() const { return m_bl; }

private:
	const BOOL m_bl;
};

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw HrException(hr);
	}
}

void ThrowIfFalse(BOOL b)
{
	if (b == FALSE)
	{
		throw BlException(b);
	}
}

std::string HrToString(HRESULT hr)
{
	char s_str[64] = { };
	sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
	return std::string(s_str);
}

