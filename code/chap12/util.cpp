#include "util.h"
#include "debug.h"

namespace Util {

static bool s_bInitialized = false;
static std::unordered_map<std::string, LoadLambda_t> s_loadLambdaTable;

void init()
{
	if (s_bInitialized)
		return;

	s_loadLambdaTable["sph"]
		= s_loadLambdaTable["spa"]
		= s_loadLambdaTable["bmp"]
		= s_loadLambdaTable["png"]
		= s_loadLambdaTable["jpg"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
		-> HRESULT
	{
		return DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
	};

	s_loadLambdaTable["tga"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
		-> HRESULT
	{
		return DirectX::LoadFromTGAFile(path.c_str(), meta, img);
	};

	s_loadLambdaTable["dds"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)
		-> HRESULT
	{
		return DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, meta, img);
	};

	s_bInitialized = true;
}

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

size_t alignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - (size % alignment);
}

std::wstring getWideStringFromString(const std::string& str)
{
	const auto num1 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0);

	std::wstring wstr;
	wstr.resize(num1);

	const auto num2 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		wstr.data(),
		num1);
	ThrowIfFalse(num1 == num2);

	return wstr;
}

std::string getExtension(const std::string& path)
{
	const size_t index = path.rfind('.');
	ThrowIfFalse(index != std::string::npos);

	return path.substr(index + 1, path.length() - index - 1);
}

std::pair<std::string, std::string> splitFileName(const std::string& path, const char splitter = '*')
{
	const size_t index = path.find(splitter);

	const std::pair<std::string, std::string> ret = {
		path.substr(0, index),
		path.substr(index + 1, path.length() - index - 1) };

	return ret;
}

std::unordered_map<std::string, LoadLambda_t> getLoadLambdaTable()
{
	ThrowIfFalse(s_bInitialized);
	return s_loadLambdaTable;
}

} // namespace Util
