#pragma once
#include <array>

enum class UiEvent {
	kUpdateAutoMovePos,
};

class Observer
{
public:
	virtual ~Observer() { }
	virtual void onNotify(UiEvent uiEvent, bool flag) = 0;
};

class Subject
{
public:
	void addObserver(Observer* observer);

protected:
	void notify(UiEvent uiEvent, bool flag);

private:
	static constexpr size_t kMaxObservers = 1;
	std::array<Observer*, kMaxObservers> m_observers = { };
	size_t m_count = 0;
};
