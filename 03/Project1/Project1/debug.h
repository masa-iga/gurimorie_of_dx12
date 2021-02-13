#pragma once
#include <cstring>
#include <stdexcept>
#include <Windows.h>

void DebugOutputFormatString(const char* format, ...);
void ThrowIfFailed(HRESULT hr);

