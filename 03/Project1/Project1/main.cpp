#include <Windows.h>
#include <cstdio>
#include <cassert>
#include <vector>
#include <tchar.h>
#include "init.h"
#include "debug.h"
#include "config.h"
#include "render.h"

using namespace std;

static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void trackFrameTime(UINT frame);

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif // _DEBUG
	DebugOutputFormatString("[Debug window]\n");
	const INT c = getchar();
	ThrowIfFalse(c != EOF);

	WNDCLASSEX w = { };
	{
		w.cbSize = sizeof(WNDCLASSEX);
		w.lpfnWndProc = (WNDPROC)WindowProcedure;
		w.lpszClassName = _T("DX12Sample");
		w.hInstance = GetModuleHandle(nullptr);
	}

	RegisterClassEx(&w);

	const long window_width = kWindowWidth;
	const long window_height = kWindowHeight;
	RECT wrc = { 0, 0, window_width, window_height };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(w.lpszClassName,
		_T("DX12_test"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr);

	ThrowIfFailed(initGraphics(hwnd));

	ShowWindow(hwnd, SW_SHOW);

	Render render;
	ThrowIfFailed(render.init());

	MSG msg = {};

	for (UINT i = 0; ; ++i)
	{
		ThrowIfFailed(render.render());

		ThrowIfFailed(render.waitForEndOfRendering());

		ThrowIfFailed(render.swap());

		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		trackFrameTime(i);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);

	return S_OK;
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return S_OK;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

class FrameCounter
{
public:
	static constexpr UINT kNumOfTrack = 600;
	FrameCounter();

	void track();
	UINT64 getInUsec() const;

private:
	LARGE_INTEGER m_freq = {};
	std::vector<LARGE_INTEGER> m_times;
	UINT m_idx = 0;
};

FrameCounter::FrameCounter()
	: m_times(kNumOfTrack)
{
	ThrowIfFalse(QueryPerformanceFrequency(&m_freq));
};

void FrameCounter::track()
{
	m_idx = (m_idx + 1) % kNumOfTrack;
	auto& time = m_times.at(m_idx);
	ThrowIfFalse(QueryPerformanceCounter(&time));
}

UINT64 FrameCounter::getInUsec() const
{
	constexpr UINT kPeriod = 60;
	static_assert(kPeriod <= kNumOfTrack);

	auto prev_idx = (m_idx + kNumOfTrack - kPeriod) % kNumOfTrack;
	const auto previous = m_times.at(prev_idx).QuadPart;
	const auto current = m_times.at(m_idx).QuadPart;

	auto elapsed = (current - previous) / kPeriod;
	elapsed *= 1'000'000; // to usec
	elapsed /= m_freq.QuadPart;

	return elapsed;
}

void trackFrameTime(UINT frame)
{
	static FrameCounter frameCounter;

	frameCounter.track();

	if (frame != 0 && frame % 60 == 0)
	{
		DebugOutputFormatString("FPS %.1f\n", 1'000'000.0f / frameCounter.getInUsec());
	}
}

