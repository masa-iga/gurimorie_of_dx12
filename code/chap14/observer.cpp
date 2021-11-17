#include "observer.h"
#include "debug.h"

void Subject::addObserver(Observer* observer)
{
	ThrowIfFalse(m_count < kMaxObservers);
	m_observers.push_back(observer);
	m_count++;
}

void Subject::removeObserver(Observer* observer)
{
	auto it = m_observers.begin();

	while (it != m_observers.end())
	{
		if ((*it) == observer)
		{
			break;
		}
		++it;
	}

	if (it == m_observers.end())
		return;

	m_observers.erase(it);
	m_count--;
}

void Subject::notify(UiEvent uiEvent, bool flag)
{
	for (auto it = m_observers.cbegin(); it != m_observers.cend(); ++it)
	{
		(*it)->onNotify(uiEvent, flag);
	}
}
