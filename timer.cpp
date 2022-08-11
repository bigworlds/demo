
#include "timer.h"

Timer::Timer()
{
	m_tickAtStart = 0;
	m_accumulatedTicks = 0;
	m_isRunning = false;
}

Timer::~Timer()
{
}

void Timer::Start()
{
	assert(!m_isRunning);
	m_isRunning = true;
	m_tickAtStart = GetTickCount();
}

void Timer::Stop()
{
	assert(m_isRunning);
	m_accumulatedTicks += GetTickCount() - m_tickAtStart;
	m_isRunning = false;
}

void Timer::Restart()
{
	Stop();
	Start();
}

double Timer::GetElapsedTime()
{
	uint64_t ticks = GetTickCount(); //tick ÀÌ ¹Ù²ð¼öµµ?
	if( ticks < m_tickAtStart)
	{
		ticks = m_tickAtStart;
	}

	return TicksToSecond(m_isRunning? ticks - m_tickAtStart : m_accumulatedTicks);
}

uint64_t Timer::InitTicksPerSecond()
{
	LARGE_INTEGER hpFrequency;
	QueryPerformanceFrequency(&hpFrequency);
	return hpFrequency.QuadPart;
}
