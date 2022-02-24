#pragma once
#include <utility>
#include "render.h"

enum class Action {
	kNone,
	kQuit,
};

namespace Input {
	Action processInputEvent(const MSG& msg, Render* pRender);
}

class InputState
{
public:
	virtual ~InputState() { }
	virtual std::pair<Action, InputState*> handleEvent(const MSG& msg, Render* pRender) = 0;

protected:
	void setX(int32_t x) { m_x = x; }
	void setY(int32_t y) { m_y = y; }
	int32_t getX() const { return m_x; }
	int32_t getY() const { return m_y; }

private:
	static int32_t m_x;
	static int32_t m_y;
};

class DefaultInputState : public InputState
{
public:
	~DefaultInputState() { }
	std::pair<Action, InputState*> handleEvent(const MSG& msg, Render* pRender) override;
};

class LbDownInputState : public InputState
{
public:
	~LbDownInputState() { }
	std::pair<Action, InputState*> handleEvent(const MSG& msg, Render* pRender) override;
};

