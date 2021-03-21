#include "debug.h"

#ifdef _DEBUG
#include <iostream>
#include <cstdarg>
#endif // _DEBUG

#define OUTPUT_DEBUG_MESSAGE_TO_VS_WINDOW (1)

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
#if OUTPUT_DEBUG_MESSAGE_TO_VS_WINDOW
	va_list valist;
	va_start(valist, format);
	char tmp[256];
	auto ret = vsprintf_s(tmp, format, valist);
	ThrowIfFalse(ret >= 0);
	OutputDebugStringA(tmp);
	va_end(valist);
#else
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // OUTPUT_DEBUG_MESSAGE_TO_VS_WINDOW
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

