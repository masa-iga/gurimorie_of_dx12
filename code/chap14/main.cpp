#pragma warning(push, 0)
#include <codeanalysis/warnings.h>
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cassert>
#include <dxgidebug.h>
#include <tchar.h>
#include <windowsx.h>
#pragma warning(pop)
#include "config.h"
#include "debug.h"
#include "imgui_if.h"
#include "init.h"
#include "input.h"
#include "loader.h"
#include "pmd_actor.h"
#include "render.h"

#pragma comment(lib, "dxguid.lib")

using namespace std;
using namespace Microsoft::WRL;

static LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void tearDown(const WNDCLASSEX& wndClass, const HWND& hwnd);
static void trackFrameTime();
static float getFps();

static uint64_t s_frame = 0;

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif // _DEBUG
	Debug::debugOutputFormatString("[Debug window]\n");

	WNDCLASSEX w = { };
	{
		w.cbSize = sizeof(WNDCLASSEX);
		w.lpfnWndProc = (WNDPROC)WindowProcedure;
		w.lpszClassName = _T("DX12Sample");
		w.hInstance = GetModuleHandle(nullptr);
	}
	ThrowIfFalse(RegisterClassEx(&w) != 0);

	const long window_width = Config::kWindowWidth;
	const long window_height = Config::kWindowHeight;
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

	Loader::init();

	ShowWindow(hwnd, SW_SHOW);

	{
		Render render;
		ThrowIfFailed(render.init(hwnd));

		MSG msg = {};

		for (s_frame = 0; ; ++s_frame)
		{
			render.setFpsInImgui(getFps());
			ThrowIfFailed(render.update());
			ThrowIfFailed(render.render());
			ThrowIfFailed(render.waitForEndOfRendering());
			ThrowIfFailed(render.swap());

			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
					break;

				if (Input::processInputEvent(msg, &render) == Action::kQuit)
				{
					render.waitForEndOfRendering();
					break;
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			trackFrameTime();
		}

		render.teardown();
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

	ImguiIf::wndProcHandler(hwnd, msg, wparam, lparam);
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static void tearDown(const WNDCLASSEX& wndClass, const HWND& hwnd)
{
	ThrowIfFalse(DestroyWindow(hwnd) != 0);

	{
		auto ret = UnregisterClass(wndClass.lpszClassName, wndClass.hInstance);

		if (ret == 0)
		{
			Debug::debugOutputFormatString("failed to unregister class. (0x%zx)\n", GetLastError());
			ThrowIfFalse(false);
		}
	}

	// release resource
	{
		ThrowIfFailed(Resource::instance()->release());
		Loader::quit();
		Resource::instance()->destroy();
	}

#ifdef _DEBUG
	{
		Debug::debugOutputFormatString("---------------- Live objects report ----------------------\n");
		ComPtr<IDXGIDebug1> dxgiDebug = nullptr;
		auto ret = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));
		ThrowIfFailed(ret);

		ret = dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		ThrowIfFailed(ret);
		Debug::debugOutputFormatString("-----------------------------------------------------------\n");
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

static FrameCounter frameCounter;

void trackFrameTime()
{
	frameCounter.track();
}

float getFps()
{
	const float fps = 1'000'000.0f / frameCounter.getInUsec();
	return fps;
}

