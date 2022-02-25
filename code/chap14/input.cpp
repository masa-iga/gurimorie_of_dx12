#include "input.h"
#include <numbers>
#include <winuser.h>

namespace {
	Action processKeyInput(const MSG& msg, Render* pRender);
	Action processMouseWheelInput(const MSG& msg, Render* pRender);
	Action processMouseLbuttonInput(const MSG& msg, Render* pRender, int32_t srcx, int32_t srcy);
	int32_t getCursorPosX();
	int32_t getCursorPosY();
}

int32_t InputState::m_x = 0;
int32_t InputState::m_y = 0;

struct StaticInstance {
	static DefaultInputState m_default;
	static LbDownInputState m_lbDown;
};
DefaultInputState StaticInstance::m_default = { };
LbDownInputState StaticInstance::m_lbDown = { };

namespace Input {
	Action processInputEvent(const MSG& msg, Render* pRender)
	{
		static InputState* s_inputState = &StaticInstance::m_default;

		const auto [action, nextState] = s_inputState->handleEvent(msg, pRender);

		if (nextState != nullptr)
		{
			s_inputState = nextState;
		}

		return action;
	}
}

std::pair<Action, InputState*> DefaultInputState::handleEvent(const MSG& msg, Render* pRender)
{
	Action action = Action::kNone;
	InputState* nextState = nullptr;

	switch (msg.message) {
	case WM_KEYDOWN:
		action = processKeyInput(msg, pRender);
		break;
	case WM_MOUSEWHEEL:
		action = processMouseWheelInput(msg, pRender);
		break;
	case WM_LBUTTONDOWN:
		nextState = &StaticInstance::m_lbDown;
		setX(getCursorPosX());
		setY(getCursorPosY());
		break;
	default:
		break;
	}

	return { action, nextState };
}

std::pair<Action, InputState*> LbDownInputState::handleEvent(const MSG& msg, Render* pRender)
{
	Action action = Action::kNone;
	InputState* nextState = nullptr;

	switch (msg.message) {
	case WM_LBUTTONUP:
		nextState = &StaticInstance::m_default;
		break;
	default:
		action = processMouseLbuttonInput(msg, pRender, getX(), getY());
		setX(getCursorPosX());
		setY(getCursorPosY());
		break;
	}

	return { action, nextState };
}

namespace {
	Action processKeyInput(const MSG& msg, Render* pRender)
	{
		switch (msg.wParam) {
		case VK_ESCAPE:
			return Action::kQuit;
		case VK_SPACE:
			pRender->toggleAnimationEnable();
			break;
		case VK_LEFT:
			pRender->moveEye(MoveEye::kFocusX, -0.03f);
			break;
		case VK_UP:
			pRender->moveEye(MoveEye::kPosY, 0.5f);
			break;
		case VK_RIGHT:
			pRender->moveEye(MoveEye::kFocusX, 0.03f);
			break;
		case VK_DOWN:
			pRender->moveEye(MoveEye::kPosY, -0.5f);
			break;
		case 'A':
			pRender->moveEye(MoveEye::kPosX, -0.03f);
			break;
		case 'W':
			pRender->moveEye(MoveEye::kPosZ, 0.5f);
			break;
		case 'D':
			pRender->moveEye(MoveEye::kPosX, 0.03f);
			break;
		case 'S':
			pRender->moveEye(MoveEye::kPosZ, -0.5f);
			break;
		case 'R':
			pRender->toggleAnimationReverse();
			break;
		default:
			break;
		}

		return Action::kNone;
	}

	Action processMouseWheelInput(const MSG& msg, Render* pRender)
	{
		constexpr float kBaseMovement = 0.5f;

		const int32_t zDelta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
		const float z = (zDelta / WHEEL_DELTA) * kBaseMovement;

		pRender->moveEye(MoveEye::kPosZ, z);

		return Action::kNone;
	}

	Action processMouseLbuttonInput(const MSG& msg, Render* pRender, int32_t prevx, int32_t prevy)
	{
		const int32_t curx = getCursorPosX();
		const int32_t cury = getCursorPosY();
		const int32_t dx = curx - prevx;
		const int32_t dy = -(cury - prevy);
		const float fx = static_cast<float>(dx) / static_cast<float>(Config::kWindowWidth) * static_cast<float>(std::numbers::pi);
		const float fy = static_cast<float>(dy) / static_cast<float>(Config::kWindowHeight) * static_cast<float>(std::numbers::pi);

		//Debug::debugOutputFormatString("[%zd] fx=%f dx=%d prevx=%d curx=%d\n", s_frame, fx, dx, prevx, curx);

		pRender->moveEye(MoveEye::kFocusX, fx);
		pRender->moveEye(MoveEye::kFocusY, fy);

		return Action::kNone;
	}

	int32_t getCursorPosX()
	{
		POINT point = { };
		GetCursorPos(&point);
		return point.x;
	}

	int32_t getCursorPosY()
	{
		POINT point = { };
		GetCursorPos(&point);
		return point.y;
	}
}
