#include "observer.h"

void Subject::addObserver(Observer* observer)
{
	m_observers.at(m_count) = observer;
	m_count++;
}

void Subject::notify(UiEvent uiEvent, bool flag)
{
	for (size_t i = 0; i < m_count; ++i)
	{
		m_observers.at(i)->onNotify(uiEvent, flag);
	}
}
