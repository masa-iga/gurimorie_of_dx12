#pragma once
#include <list>

enum class UiEvent {
	kUpdateAutoMovePos,
	kUpdateAutoLightPos,
	kUpdateHighLuminanceThreshold,
};

struct UiEventDataUpdateAutoMovePos {
	bool flag = false;
};

struct UiEventDataUpdateAutoLightPos {
	bool flag = false;
};

struct UiEventDataUpdateHighLuminanceThreshold {
	float val = 0.0f;
};

class Observer
{
public:
	virtual ~Observer() { }
	virtual void onNotify(UiEvent uiEvent, const void* uiEventData) = 0;
};

class Subject
{
public:
	void addObserver(Observer* observer);
	void removeObserver(Observer* observer);

protected:
	void notify(UiEvent uiEvent, const void* uiEventData);

private:
	static constexpr size_t kMaxObservers = 3;
	std::list<Observer*> m_observers = { };
	size_t m_count = 0;
};
