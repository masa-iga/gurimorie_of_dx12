#include <Windows.h>
#include <array>
#include <cstdio>
#include <cassert>
#include <dxgidebug.h>
#include <tchar.h>
#include "init.h"
#include "debug.h"
#include "config.h"
#include "render.h"

#pragma comment(lib, "dxguid.lib")

using namespace std;

enum class Action {
	kNone,
	kQuit,
};

static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static Action processKeyInput(const MSG& msg, Render* pRender);
static void tearDown(const WNDCLASSEX& wndClass, const HWND& hwnd);
static void trackFrameTime(UINT frame);

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif // _DEBUG
	DebugOutputFormatString("[Debug window]\n");

	WNDCLASSEX w = { };
	{
		w.cbSize = sizeof(WNDCLASSEX);
		w.lpfnWndProc = (WNDPROC)WindowProcedure;
		w.lpszClassName = _T("DX12Sample");
		w.hInstance = GetModuleHandle(nullptr);
	}
	ThrowIfFalse(RegisterClassEx(&w) != 0);

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

	Resource::create();
	ThrowIfFailed(Resource::instance()->allocate(hwnd));

	ShowWindow(hwnd, SW_SHOW);

	{
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

			if (processKeyInput(msg, &render) == Action::kQuit)
			{
				render.waitForEndOfRendering();
				break;
			}

			trackFrameTime(i);
		}
	}

	tearDown(w, hwnd);

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

static Action processKeyInput(const MSG& msg, Render* pRender)
{
	switch (msg.message) {
	case WM_KEYDOWN:
		switch (msg.wParam) {
		case VK_ESCAPE:
			return Action::kQuit;
		case VK_SPACE:
			pRender->toggleAnimationEnable();
			break;
		case 'R':
			pRender->toggleAnimationReverse();
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return Action::kNone;
}

static void tearDown(const WNDCLASSEX& wndClass, const HWND& hwnd)
{
	ThrowIfFalse(DestroyWindow(hwnd) != 0);

	{
		auto ret = UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);

		if (ret == 0)
		{
			DebugOutputFormatString("failed to unregister class. (0x%zx)\n", GetLastError());
			ThrowIfFalse(false);
		}
	}

	ThrowIfFailed(Resource::instance()->release());
	Resource::instance()->destroy();

#ifdef _DEBUG
	{
		DebugOutputFormatString("-----------------------------------------------------------\n");
		Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
		auto ret = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
		ThrowIfFailed(ret);

		ret = dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		ThrowIfFailed(ret);
		DebugOutputFormatString("-----------------------------------------------------------\n");
	}
#endif // _DEBUG
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
	std::array<LARGE_INTEGER, kNumOfTrack> m_times;
	UINT m_idx = 0;
};

FrameCounter::FrameCounter()
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

	const auto prev_idx = (m_idx + kNumOfTrack - kPeriod) % kNumOfTrack;
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

