#include "observer.h"

inline void Subject::addObserver(Observer* observer)
{
	m_observers.at(m_count) = observer;
	m_count++;
}
