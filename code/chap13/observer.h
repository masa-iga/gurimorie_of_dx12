#pragma once
#include <array>

class Observer
{
public:
	virtual ~Observer() { }
	virtual void onNotify() = 0;
};

class Subject
{
public:
	void addObserver(Observer* observer);

private:
	static constexpr size_t kMaxObservers = 1;
	std::array<Observer*, kMaxObservers> m_observers = { };
	size_t m_count = 0;
};
