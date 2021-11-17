#pragma once
#include <list>

enum class UiEvent {
	kUpdateAutoMovePos,
	kUpdateAutoLightPos,
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
	void removeObserver(Observer* observer);

protected:
	void notify(UiEvent uiEvent, bool flag);

private:
	static constexpr size_t kMaxObservers = 3;
	std::list<Observer*> m_observers = { };
	size_t m_count = 0;
};
