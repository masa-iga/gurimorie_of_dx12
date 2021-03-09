#pragma once
#include <Windows.h>
#include <string>

class TimeCounter
{
public:
	TimeCounter(std::string str = "");
	~TimeCounter();

private:
	LARGE_INTEGER m_start = {};
	std::string m_str = "";
};

